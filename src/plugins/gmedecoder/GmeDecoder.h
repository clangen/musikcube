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

/// @file GmeDecoder.h
/// @brief Decoder for video-game music (chiptune) files built on Game Music Emu.
/// @details Supports NSF, SPC, VGM, GYM, GBS, HES, KSS, SAP, AY and NSFE
/// archives. Tracks may be looped or faded out according to the plugin
/// settings, and multi-track archives select a sub-track through the GME URI.

#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IDecoder.h>
#include <musikcore/sdk/IDataStream.h>
#include "GmeDataStream.h"
#include <stddef.h>
#include <gme/gme.h>
#include <mutex>

using namespace musik::core::sdk;

/** @brief Decodes video-game music archives using the Game Music Emu library.
 *  @details Owns a Music_Emu handle and renders 16-bit signed stereo PCM at
 *  44.1 kHz. Reads are guarded by a mutex and driven by the settings exposed
 *  in Constants.h (looping, fade-out, track length). */
class GmeDecoder: public musik::core::sdk::IDecoder {
    public:
        /** @brief Constructs a decoder with no open track. */
        GmeDecoder();
        /** @brief Destroys the decoder and frees the emulator handle. */
        virtual ~GmeDecoder();

        /** @brief Destroys the decoder instance. */
        void Release() override;
        /** @brief Seeks to a position in the current track.
         *  @param seconds Target position in seconds.
         *  @return The actual position reached, in seconds. */
        double SetPosition(double seconds) override;
        /** @brief Fills a buffer with the next block of decoded PCM.
         *  @param buffer The buffer to fill.
         *  @return True if data was written, false at end of track. */
        bool GetBuffer(IBuffer *buffer) override;
        /** @brief Returns the total duration of the current track.
         *  @return Duration in seconds. */
        double GetDuration() override;
        /** @brief Opens the decoder against a GME data stream.
         *  @param stream The GmeDataStream to decode.
         *  @return True if the track opened successfully. */
        bool Open(musik::core::sdk::IDataStream *stream) override;
        /** @brief Returns whether the track has been fully consumed.
         *  @return True when exhausted. */
        bool Exhausted() override;
        /** @brief GME always decodes at its native rate.
         *  @param rate Requested rate (ignored). */
        void SetPreferredSampleRate(int rate) override { }

    private:
        /** @brief The data stream being decoded. */
        GmeDataStream* stream { nullptr };
        /** @brief Game Music Emu playback handle. */
        Music_Emu* gme { nullptr };
        /** @brief Info block describing the currently loaded track. */
        gme_info_t* info { nullptr };
        /** @brief Scratch buffer for decoded PCM samples. */
        short* buffer;
        /** @brief Effective track length in seconds. */
        double length{ -1.0 };
        /** @brief Total number of samples in the track. */
        int totalSamples { 0 };
        /** @brief Number of samples already played. */
        int samplesPlayed { 0 };
        /** @brief True once the track is fully consumed. */
        bool exhausted { false };
        /** @brief Whether the stream is owned by the decoder. */
        bool isWrappedDataStream{ false };
        /** @brief Guards access to the emulator state. */
        std::mutex mutex;
};
