//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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
#include "TranscodingDataStream.h"
#include "Constants.h"
#include "Util.h"
#include <boost/filesystem.hpp>

using namespace musik::core::sdk;
using namespace boost::filesystem;

static std::string cachePath(Context& context) {
    char buf[4096];
    context.environment->GetPath(PathType::PathData, buf, sizeof(buf));
    std::string path = std::string(buf) + "/cache/transcoder/";
	boost::filesystem::path boostPath(path);
    if (!exists(boostPath)) {
        create_directories(boostPath);
    }

    return path;
}

static void iterateTranscodeCache(Context& context, std::function<void(path)> cb) {
    if (cb) {
        directory_iterator end;
        directory_iterator file(cachePath(context));

        while (file != end) {
            if (!is_directory(file->status())) {
                cb(file->path());
            }
            ++file;
        }
    }
}

void Transcoder::RemoveTempTranscodeFiles(Context& context) {
    iterateTranscodeCache(context, [](path p) {
        if (p.extension().string() == ".tmp") {
            boost::system::error_code ec;
            remove(p, ec);
        }
    });
}

void Transcoder::PruneTranscodeCache(Context& context) {
    std::map<time_t, path> sorted;

    boost::system::error_code ec;
    iterateTranscodeCache(context, [&sorted, &ec](path p) {
        sorted[last_write_time(p, ec)] = p;
    });

    int maxSize = context.prefs->GetInt(
        prefs::transcoder_cache_count.c_str(),
        defaults::transcoder_cache_count);

    int extra = (int) sorted.size() - (maxSize - 1);
    auto it = sorted.begin();
    while (extra > 0 && it != sorted.end()) {
        auto p = it->second;
        boost::system::error_code ec;
        if (remove(p, ec)) {
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
    finalFn = std::string(
        cachePath(context) +
        std::to_string(std::hash<std::string>()(uri)) +
        "-" + std::to_string(bitrate) +
        "." + format);

    do {
        tempFn = finalFn + "." + std::to_string(rand()) + ".tmp";
    } while (exists(tempFn));
}

IDataStream* Transcoder::Transcode(
    Context& context,
    const std::string& uri,
    size_t bitrate,
    const std::string& format)
{
    if (context.prefs->GetBool(
        prefs::transcoder_synchronous.c_str(),
        defaults::transcoder_synchronous))
    {
        return TranscodeAndWait(context, uri, bitrate, format);
    }

    /* on-demand is the default. */
    return TranscodeOnDemand(context, uri, bitrate, format);
}

IDataStream* Transcoder::TranscodeOnDemand(
    Context& context,
    const std::string& uri,
    size_t bitrate,
    const std::string& format)
{
    /* see if it already exists in the cache. if it does, just return it. */
    std::string expectedFilename, tempFilename;
    getTempAndFinalFilename(context, uri, bitrate, format, tempFilename, expectedFilename);

    if (exists(expectedFilename)) {
        boost::system::error_code ec;
        last_write_time(expectedFilename, time(nullptr), ec);
        return context.environment->GetDataStream(expectedFilename.c_str());
    }

    /* if it doesn't exist, check to see if the cache is enabled. */
    int cacheCount = context.prefs->GetInt(
        prefs::transcoder_cache_count.c_str(),
        defaults::transcoder_cache_count);

    TranscodingDataStream* transcoder = nullptr;

    if (cacheCount > 0) {
        PruneTranscodeCache(context);

        transcoder = new TranscodingDataStream(
            context, uri, tempFilename, expectedFilename, bitrate, format);

        /* if the stream has an indeterminite length, close it down and
        re-open it without caching options; we don't want to fill up
        the storage disk */
        if (transcoder->Length() < 0) {
            transcoder->Release();
            delete transcoder;
            transcoder = new TranscodingDataStream(context, uri, bitrate, format);
        }
    }
    else {
        transcoder = new TranscodingDataStream(context, uri, bitrate, format);
    }

    return transcoder;
}

IDataStream* Transcoder::TranscodeAndWait(
    Context& context,
    const std::string& uri,
    size_t bitrate,
    const std::string& format)
{
    std::string expectedFilename, tempFilename;
    getTempAndFinalFilename(context, uri, bitrate, format, tempFilename, expectedFilename);

    /* already exists? */
    if (exists(expectedFilename)) {
        boost::system::error_code ec;
        last_write_time(expectedFilename, time(nullptr), ec);
        return context.environment->GetDataStream(expectedFilename.c_str());
    }

    TranscodingDataStream* transcoder = new TranscodingDataStream(
        context, uri, tempFilename, expectedFilename, bitrate, format);

    /* transcoders with a negative length have an indeterminate duration, so
    we disallow waiting for them because they may never finish */
    if (transcoder->Length() < 0) {
        transcoder->Release();
        delete transcoder;
        return nullptr;
    }

    char buffer[8192];
    while (!transcoder->Eof()) {
        transcoder->Read(buffer, sizeof(buffer));
        std::this_thread::yield();
    }

    transcoder->Release();
    PruneTranscodeCache(context);

    return context.environment->GetDataStream(uri.c_str());
}