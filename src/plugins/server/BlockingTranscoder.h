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

///

/// @file BlockingTranscoder.h
/// @brief Offline audio transcoder used by the streaming server.
/// @details Runs a blocking transcode of a URI into a temporary file, then
/// atomically moves the result to its final cache location. Used for formats
/// that cannot be transcoded on-demand in real time.

#include <musikcore/sdk/IDataStream.h>
#include <musikcore/sdk/IBlockingEncoder.h>
#include <musikcore/sdk/DataBuffer.h>
#include "Context.h"
#include <thread>
#include <condition_variable>
#include <mutex>
#include <string>
#include <stdio.h>

/** @brief Performs a blocking audio transcode into the server cache.
 *  @details Opens the source URI through the context's stream factory, drives
 *  an IBlockingEncoder to write the full transcoded result to a temporary
 *  file, then renames it to the final filename. The number of active instances
 *  is tracked so the server can limit concurrent transcodes. */
class BlockingTranscoder {
    public:
        /** @brief Position type alias. */
        using PositionType = musik::core::sdk::PositionType;

        /** @brief Constructs a transcode task.
         *  @param context Shared server context.
         *  @param encoder The blocking encoder to use.
         *  @param uri URI of the source track.
         *  @param tempFilename Temporary output file.
         *  @param finalFilename Final cached output file.
         *  @param bitrate Target bitrate. */
        BlockingTranscoder(
            Context& context,
            musik::core::sdk::IBlockingEncoder* encoder,
            const std::string& uri,
            const std::string& tempFilename,
            const std::string& finalFilename,
            int bitrate);

        /** @brief Destroys the transcode task. */
        virtual ~BlockingTranscoder();

        /** @brief Runs the transcode to completion.
         *  @return True if the transcode succeeded. */
        bool Transcode();
        /** @brief Requests that a running transcode be aborted. */
        void Interrupt();

        /** @brief Returns the number of active transcode tasks.
         *  @return Active task count. */
        static int GetActiveCount();

    private:
        /** @brief Releases resources and removes partial output files. */
        void Cleanup();

        /** @brief Shared server context. */
        Context& context;
        /** @brief Input stream opened on the source URI. */
        musik::core::sdk::IDataStream* input;
        /** @brief The blocking encoder being driven. */
        musik::core::sdk::IBlockingEncoder* encoder;
        /** @brief Output stream for the temp file. */
        musik::core::sdk::IDataStream* output;
        /** @brief Temporary and final cache file names. */
        std::string tempFilename, finalFilename;
        /** @brief Target bitrate. */
        int bitrate;
        /** @brief Whether the task was interrupted. */
        bool interrupted;
};