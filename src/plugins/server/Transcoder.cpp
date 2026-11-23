//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "Transcoder.h"
#include "musikcore/sdk/Filesystem.h"
#include "BlockingTranscoder.h"
#include "TranscodingAudioDataStream.h"
#include "Constants.h"
#include "Util.h"
#include <musikcore/sdk/IBlockingEncoder.h>

#include <thread>
#include <set>
#include <atomic>
#include <condition_variable>
#include <map>
#include <filesystem>
#include <functional>
#include <chrono>

using musik::core::sdk::fs::pathToUtf8;

namespace fs = std::filesystem;

using namespace std::chrono;
using musik::core::sdk::IBlockingEncoder;
using server::EncoderOwner;
using musik::core::sdk::IEncoder;
using musik::core::sdk::IStreamingEncoder;

using musik::core::sdk::PathType;

static std::mutex transcoderMutex;
static std::condition_variable waitForTranscode;
static std::set<std::string> runningBlockingTranscoders;

static IEncoder* getEncoder(Context& context, const std::string& format) {
    std::string extension = "." + format;
    return context.environment->GetEncoder(extension.c_str());
}

static std::string cachePath(Context& context) {
    char buf[4096];
    context.environment->GetPath(PathType::Data, buf, sizeof(buf));
    fs::path fsPath = fs::u8path(buf) / "cache" / "transcoder";
    if (!fs::exists(fsPath)) {
        fs::create_directories(fsPath);
    }
    return pathToUtf8(fsPath);
}

static void iterateTranscodeCache(Context& context, std::function<void(fs::path)> cb) {
    if (cb) {
        fs::directory_iterator end;
        fs::directory_iterator file(fs::u8path(cachePath(context)));
        while (file != end) {
            if (!is_directory(file->status())) {
                cb(file->path());
            }
            ++file;
        }
    }
}

void Transcoder::RemoveTempTranscodeFiles(Context& context) {
    iterateTranscodeCache(context, [](fs::path p) {
        if (pathToUtf8(p.extension()) == ".tmp") {
            std::error_code ec;
            fs::remove(p, ec);
        }
    });
}

void Transcoder::PruneTranscodeCache(Context& context) {
    std::multimap<fs::file_time_type, fs::path> sorted;

    iterateTranscodeCache(context, [&sorted](fs::path p) {
        if (p.extension() != ".tmp") {
            std::error_code error;
            const auto written = fs::last_write_time(p, error);
            if (!error) {
                sorted.emplace(written, p);
            }
        }
    });

    int maxSize = context.prefs->GetInt(
        serverprefs::CacheCount.c_str(),
        serverprefs::DefaultCacheCount);

    int extra = (int) sorted.size() - (maxSize - 1);
    auto it = sorted.begin();
    while (extra > 0 && it != sorted.end()) {
        auto p = it->second;
        std::error_code ec;
        if (fs::remove(p, ec)) {
            --extra;
        }
        ++it;
    }
}

static void getTempAndFinalFilename(
    Context& context,
    const std::string& uri,
    size_t bitrate,
    const std::string& format,
    std::string& tempFn,
    std::string& finalFn)
{
    const auto leaf = std::to_string(std::hash<std::string>()(uri)) + "-" + std::to_string(bitrate) + "." + format;
    finalFn = pathToUtf8(fs::u8path(cachePath(context)) / fs::u8path(leaf));

    static std::atomic<uint64_t> sequence { 0 };
    do {
        tempFn = finalFn + "." + std::to_string(sequence.fetch_add(1)) + ".tmp";
    } while (fs::exists(fs::u8path(tempFn)));
}

std::unique_ptr<server::ResponseBody> Transcoder::Transcode(
    Context& context, const std::string& uri, size_t bitrate, const std::string& format, Request request)
{
    if (format.empty() || format.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") !=
        std::string::npos)
    {
        return {};
    }

    EncoderOwner<IEncoder> encoder(getEncoder(context, format));
    if (!encoder) {
        return {};
    }
    std::string temp, destination;
    getTempAndFinalFilename(context, uri, bitrate, format, temp, destination);
    const auto path = fs::u8path(destination);
    if (fs::exists(path)) {
        if (auto cached = server::OpenFileBody(context, destination)) {
            std::error_code error;
            fs::last_write_time(path, fs::file_time_type::clock::now(), error);
            return cached;
        }
    }

    if (auto* streaming = encoder->AsStreaming()) {
        const bool synchronous = context.prefs->GetBool(serverprefs::SynchronousTranscoding.c_str(),
            defaults::transcoder_synchronous);
        const bool wait = synchronous || request == Request::CompleteFile || (request == Request::PartialTrack &&
            context.prefs->GetBool(prefs::transcoder_synchronous_fallback.c_str(),
                defaults::transcoder_synchronous_fallback));
        const bool cache = context.prefs->GetInt(serverprefs::CacheCount.c_str(),
            serverprefs::DefaultCacheCount) > 0;
        if (cache) {
            PruneTranscodeCache(context);
        }

        encoder.release();
        EncoderOwner<IStreamingEncoder> owner(streaming);
        auto body = std::make_unique<TranscodingAudioDataStream>(std::move(owner), bitrate);
        if (!body->Open(context, uri, synchronous, (cache || wait) ? temp : "", cache ? destination : "")) {
            return {};
        }
        /* an unknown-duration source might never finish. let asynchronous
        clients read it live even when they normally need an exact length. */
        if (!wait || !body->IsCaching()) {
            return body;
        }

        /* wait-mode must serve the completed encoded file, even when the
        selected codec can stream. a failed encode never publishes a cache. */
        return body->Finish(context);
    }

    auto* blocking = encoder->AsBlocking();
    if (!blocking) {
        return {};
    }
    {
        /* requests for the same encoding share one completed file. other
        formats or bitrates of the same source can progress independently. */
        std::unique_lock lock(transcoderMutex);
        if (!runningBlockingTranscoders.insert(destination).second) {
            waitForTranscode.wait(lock, [&] { return !runningBlockingTranscoders.count(destination); });
            lock.unlock();
            return server::OpenFileBody(context, destination);
        }
    }
    struct Completion {
        const std::string& destination;
        ~Completion() {
            std::lock_guard lock(transcoderMutex);
            runningBlockingTranscoders.erase(destination);
            waitForTranscode.notify_all();
        }
    } completion {
        .destination = destination,
    };
    PruneTranscodeCache(context);
    if (!BlockingTranscoder::Run(context, *blocking, uri, temp, destination, bitrate)) {
        return {};
    }
    return server::OpenFileBody(context, destination);
}

int Transcoder::GetActiveCount() {
    return BlockingTranscoder::GetActiveCount() + TranscodingAudioDataStream::GetActiveCount();
}
