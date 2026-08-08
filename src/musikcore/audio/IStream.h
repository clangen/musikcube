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

/** @file IStream.h
 *  @brief Abstract interface for a streaming audio source feeding a Player.
 *  @details A stream wraps a decoder plus an optional chain of DSP plugins and
 *      exposes processed output buffers to the player on demand. */

#include <musikcore/config.h>
#include <musikcore/sdk/IBuffer.h>
#include <musikcore/sdk/IDecoder.h>
#include <musikcore/sdk/IDSP.h>
#include <musikcore/sdk/IDecoderFactory.h>
#include <musikcore/sdk/IOutput.h>

#include <list>

/** @namespace musik::core::audio
 *  @brief Audio pipeline types: buffers, players, transports and streams. */
namespace musik { namespace core { namespace audio {

    /** @brief Interface for an audio stream consumed by a Player.
     *  @details Implementations decode a URI, run the audio through any configured
     *      DSP plugins and hand processed interleaved buffers to the player. The
     *      player must return each buffer via OnBufferProcessedByPlayer() so it can
     *      be recycled. */
    class IStream {
        public:
            /** @brief Returns the next fully processed output buffer.
             *  @return A buffer with decoded+DSP processed samples, or nullptr on
             *      EOF/error. Ownership remains with the stream. */
            virtual musik::core::sdk::IBuffer* GetNextProcessedOutputBuffer() = 0;
            /** @brief Returns a processed buffer to the stream for recycling.
             *  @param buffer The buffer previously returned by GetNextProcessedOutputBuffer(). */
            virtual void OnBufferProcessedByPlayer(musik::core::sdk::IBuffer* buffer) = 0;
            /** @brief Seeks within the stream.
             *  @param seconds The target position, in seconds.
             *  @return The actual position achieved, in seconds. */
            virtual double SetPosition(double seconds) = 0;
            /** @return The total stream duration, in seconds. */
            virtual double GetDuration() = 0;
            /** @brief Opens the stream for the given URI.
             *  @param uri The URI to open (file, URL, etc.).
             *  @param output The output device the stream formats buffers for.
             *  @return true on success, false if the stream could not be opened. */
            virtual bool OpenStream(std::string uri, musik::core::sdk::IOutput* output) = 0;
            /** @brief Asynchronously interrupts playback (e.g. on Stop). */
            virtual void Interrupt() = 0;
            /** @return A bitmask of sdk::Capability values supported by the stream. */
            virtual int GetCapabilities() = 0;
            /** @return true when the end of the stream has been reached. */
            virtual bool Eof() = 0;
            /** @brief Frees the stream. Implementations typically delete themselves. */
            virtual void Release() = 0;
    };

    typedef std::shared_ptr<IStream> IStreamPtr; /**< Shared ownership alias for IStream. */

} } }
