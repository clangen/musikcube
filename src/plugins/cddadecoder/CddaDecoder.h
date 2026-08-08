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

/// @file CddaDecoder.h
/// @brief Decoder that plays raw audio-CD PCM from a CddaDataStream.
/// @details Reads 16-bit stereo PCM directly from the sectors of an audio CD
/// and exposes it through the decoder SDK without any resampling. Windows-only.

#include <musikcore/sdk/IDecoder.h>
#include "CddaDataStream.h"

using namespace musik::core::sdk;

/** @brief Decodes raw audio-CD tracks.
 *  @details The input is a CddaDataStream opened on a specific track. PCM is
 *  passed through untouched at the native CD sample rate (44100 Hz). */
class CddaDecoder : public IDecoder {
    public:
        CddaDecoder();
        ~CddaDecoder();

        /** @brief Opens the decoder against a CDDA data stream.
         *  @param data The CddaDataStream for the track to play.
         *  @return True if the decoder opened successfully. */
        bool Open(IDataStream* data) override;
        /** @brief Destroys the decoder. */
        void Release() override;
        /** @brief Seeks to a position in the track.
         *  @param seconds Target position in seconds.
         *  @return The actual position reached, in seconds. */
        double SetPosition(double seconds) override;
        /** @brief Returns the total duration of the track.
         *  @return Duration in seconds. */
        double GetDuration() override;
        /** @brief Fills a buffer with the next block of raw PCM.
         *  @param buffer The buffer to fill.
         *  @return True if data was written. */
        bool GetBuffer(IBuffer *buffer) override;
        /** @brief Returns whether the track has been fully read.
         *  @return True when exhausted. */
        bool Exhausted() noexcept override { return this->exhausted; }
        /** @brief The output rate is fixed to the CD sample rate.
         *  @param rate Requested rate (ignored). */
        void SetPreferredSampleRate(int rate) override { }

    private:
        /** @brief The CDDA stream being decoded. */
        CddaDataStream* data;
        /** @brief Track duration in seconds. */
        double duration;
        /** @brief Scratch buffer for raw sector reads. */
        BYTE* buffer;
        /** @brief True once the track is fully consumed. */
        bool exhausted;
};
