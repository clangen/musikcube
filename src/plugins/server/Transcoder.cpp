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
#include "BlockingTranscoder.h"
#include "TranscodingAudioDataStream.h"
#include "Constants.h"
#include "Util.h"
#include <core/sdk/IDataStreamEncoder.h>
#include <boost/filesystem.hpp>

using namespace musik::core::sdk;
using namespace boost::filesystem;

static IEncoder* getEncoder(Context& context, const std::string& format) {
    std::string extension = "." + format;
    return context.environment->GetEncoder(extension.c_str());
}

template <typename T>
static T* getTypedEncoder(Context& context, const std::string& format) {
    IEncoder* encoder = getEncoder(context, format);
    if (encoder) {
        T* typedEncoder = dynamic_cast<T*>(encoder);
        if (typedEncoder) {
            return typedEncoder;
        }
        encoder->Release();
    }
    return nullptr;
}

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
        return TranscodeAndWait(context, getEncoder(context, format), uri, bitrate, format);
    }

    /* on-demand is the default. however, on-demand transcoding is only available
    for `IAudioStreamEncoder` types.  */
    IAudioStreamEncoder* audioStreamEncoder = getTypedEncoder<IAudioStreamEncoder>(context, format);
    if (audioStreamEncoder) {
        return TranscodeOnDemand(context, audioStreamEncoder, uri, bitrate, format);
    }

    return nullptr;
}

IDataStream* Transcoder::TranscodeOnDemand(
    Context& context,
    IAudioStreamEncoder* encoder,
    const std::string& uri,
    size_t bitrate,
    const std::string& format)
{
    /* the caller can specify an encoder; if it is not specified, go ahead and
    create one here */
    if (!encoder) {
        encoder = getTypedEncoder<IAudioStreamEncoder>(context, format);
        if (!encoder) {
            return nullptr;
        }
    }

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

    TranscodingAudioDataStream* transcoderStream = nullptr;

    if (cacheCount > 0) {
        PruneTranscodeCache(context);

        transcoderStream = new TranscodingAudioDataStream(
            context, encoder, uri, tempFilename, expectedFilename, bitrate, format);

        /* if the stream has an indeterminite length, close it down and
        re-open it without caching options; we don't want to fill up
        the storage disk */
        if (transcoderStream->Length() < 0) {
            transcoderStream->Release();
            delete transcoderStream;
            transcoderStream = new TranscodingAudioDataStream(context, encoder, uri, bitrate, format);
        }
    }
    else {
        transcoderStream = new TranscodingAudioDataStream(context, encoder, uri, bitrate, format);
    }

    return transcoderStream;
}

IDataStream* Transcoder::TranscodeAndWait(
    Context& context,
    IEncoder* encoder,
    const std::string& uri,
    size_t bitrate,
    const std::string& format)
{
    /* the caller can specify an encoder; if it is not specified, go ahead and
    create one here */
    if (!encoder) {
        encoder = getEncoder(context, format);
        if (!encoder) {
            return nullptr;
        }
    }

    std::string expectedFilename, tempFilename;
    getTempAndFinalFilename(context, uri, bitrate, format, tempFilename, expectedFilename);

    /* already exists? */
    if (exists(expectedFilename)) {
        boost::system::error_code ec;
        last_write_time(expectedFilename, time(nullptr), ec);
        return context.environment->GetDataStream(expectedFilename.c_str());
    }

    IAudioStreamEncoder* audioStreamEncoder = dynamic_cast<IAudioStreamEncoder*>(encoder);
    if (audioStreamEncoder) {
        TranscodingAudioDataStream* transcoderStream = new TranscodingAudioDataStream(
            context, audioStreamEncoder, uri, tempFilename, expectedFilename, bitrate, format);

        /* transcoders with a negative length have an indeterminate duration, so
        we disallow waiting for them because they may never finish */
        if (transcoderStream->Length() < 0) {
            transcoderStream->Release();
            delete transcoderStream;
            return nullptr;
        }

        char buffer[8192];
        while (!transcoderStream->Eof()) {
            transcoderStream->Read(buffer, sizeof(buffer));
            std::this_thread::yield();
        }

        transcoderStream->Release();
        PruneTranscodeCache(context);
        return context.environment->GetDataStream(uri.c_str());
    }
    else {
        IDataStreamEncoder* dataStreamEncoder = dynamic_cast<IDataStreamEncoder*>(encoder);
        if (dataStreamEncoder) {
            BlockingTranscoder blockingTranscoder(
                context, dataStreamEncoder, uri, tempFilename, expectedFilename);

            if (!blockingTranscoder.Transcode()) {
                return nullptr;
            }
        }

        PruneTranscodeCache(context);
        return context.environment->GetDataStream(expectedFilename.c_str());
    }
}