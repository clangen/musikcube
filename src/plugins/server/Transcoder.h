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

#pragma once

/// @file Transcoder.h
/// @brief Transcoding service for the streaming server.
/// @details Provides on-demand (streaming) transcoding for formats that can be
/// converted in real time, and blocking transcoding for formats that cannot.
/// Transcodes are cached on disk and the cache is pruned to a bounded size.

#include "Context.h"
#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IDataStream.h>
#include <musikcore/sdk/IDecoder.h>
#include <musikcore/sdk/IStreamingEncoder.h>
#include <string>

/** @brief Static helper service for transcoding tracks for remote clients.
 *  @details Chooses between an on-demand streaming transcode (real-time) and a
 *  blocking transcode (offline) based on the encoder type and server settings.
 *  Cache files are written to a temp location and atomically renamed. */
class Transcoder {
    public:
        /** @brief Data stream type alias. */
        using IDataStream = musik::core::sdk::IDataStream;
        /** @brief Encoder type alias. */
        using IEncoder = musik::core::sdk::IEncoder;
        /** @brief Streaming encoder type alias. */
        using IStreamingEncoder = musik::core::sdk::IStreamingEncoder;

        /** @brief Removes temporary transcode files from the cache directory.
         *  @param context Shared server context. */
        static void RemoveTempTranscodeFiles(Context& context);

        /** @brief Prunes the transcode cache to its configured size.
         *  @param context Shared server context. */
        static void PruneTranscodeCache(Context& context);

        /** @brief Transcodes a URI to the requested format.
         *  @details Selects an encoder factory for the format and returns a
         *  readable stream of transcoded data.
         *  @param context Shared server context.
         *  @param uri URI of the source track.
         *  @param bitrate Target bitrate.
         *  @param format Target format name.
         *  @return A data stream of transcoded audio, or null on failure. */
        static IDataStream* Transcode(
            Context& context,
            const std::string& uri,
            size_t bitrate,
            const std::string& format);

        /** @brief Transcodes a URI and waits for the result.
         *  @details Runs a blocking transcode and returns a stream that reads
         *  the finished file, or null if the transcode failed.
         *  @param context Shared server context.
         *  @param encoder The encoder to drive.
         *  @param uri URI of the source track.
         *  @param bitrate Target bitrate.
         *  @param format Target format name.
         *  @return A data stream over the transcoded file, or null. */
        static IDataStream* TranscodeAndWait(
            Context& context,
            IEncoder* encoder,
            const std::string& uri,
            size_t bitrate,
            const std::string& format);

        /** @brief Returns the number of active transcode tasks.
         *  @return Active task count. */
        static int GetActiveCount();

    private:
        /** @brief Starts a real-time streaming transcode.
         *  @param context Shared server context.
         *  @param encoder The streaming encoder to drive.
         *  @param uri URI of the source track.
         *  @param bitrate Target bitrate.
         *  @param format Target format name.
         *  @return A data stream of transcoded audio, or null. */
        static IDataStream* TranscodeOnDemand(
            Context& context,
            IStreamingEncoder* encoder,
            const std::string& uri,
            size_t bitrate,
            const std::string& format);

        /** @brief Private constructor (static-only class). */
        Transcoder() { }
        /** @brief Private destructor (static-only class). */
        ~Transcoder() { }
};