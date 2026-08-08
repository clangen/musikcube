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

/** @file IDecoder.h @brief Defines the IDecoder interface for decoding audio streams into PCM buffers. */
#pragma once

#include "IDataStream.h"
#include "IBuffer.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief Decodes audio from an IDataStream into interleaved float PCM
     *  buffers for the playback pipeline. */
    class IDecoder {
        public:
            /** @brief Releases the decoder; callers must invoke this when done. */
            virtual void Release() = 0;

            /** @brief Seeks to an absolute position in the stream.
             *  @param seconds The target position, in seconds.
             *  @return The actual position reached, in seconds. */
            virtual double SetPosition(double seconds) = 0;

            /** @brief Decodes the next chunk of audio into the given buffer.
             *  @param buffer The buffer to fill with decoded PCM data.
             *  @return True if a buffer was produced. */
            virtual bool GetBuffer(IBuffer *buffer) = 0;

            /** @brief Returns the total duration of the stream.
             *  @return The duration in seconds, or a negative value if unknown. */
            virtual double GetDuration() = 0;

            /** @brief Opens the given stream for decoding.
             *  @param stream The stream to decode.
             *  @return True if the stream was opened successfully. */
            virtual bool Open(IDataStream *stream) = 0;

            /** @brief Indicates whether all audio in the stream has been decoded.
             *  @return True when the stream is fully consumed. */
            virtual bool Exhausted() = 0;

            /** @brief Requests the decoder to output audio at a preferred rate.
             *  @param rate The preferred sample rate in Hz. */
            virtual void SetPreferredSampleRate(int rate) = 0;
    };

} } }
