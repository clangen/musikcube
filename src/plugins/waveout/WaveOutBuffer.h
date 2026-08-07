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

/// @file WaveOutBuffer.h
/// @brief Wraps a PCM buffer for playback through the waveOut API.
/// @details A WaveOutBuffer associates an IBuffer with the WAVEHDR header
/// needed to enqueue it to the waveOut device. When the device finishes
/// playing the header, the buffer is released back to its provider.

#include "config.h"
#include <musikcore/sdk/IBuffer.h>
#include <musikcore/sdk/IBufferProvider.h>

class WaveOut;

using namespace musik::core::sdk;

/** @brief Adapts a PCM buffer to a waveOut WAVEHDR.
 *  @details Holds the wrapped IBuffer, its provider and the WAVEHDR structure
 *  passed to waveOutWrite. WriteToOutput() enqueues the buffer to the device;
 *  when the device reports completion the provider is notified. */
class WaveOutBuffer {
    public:
        /** @brief Constructs a buffer owned by the given output.
         *  @param waveOut The owning WaveOut output. */
        WaveOutBuffer(WaveOut *waveOut);
        /** @brief Destroys the buffer. */
        ~WaveOutBuffer();

        /** @brief Associates a PCM buffer and its provider.
         *  @param buffer The PCM buffer to play.
         *  @param provider The provider owning the buffer. */
        void Set(IBuffer *buffer, IBufferProvider *provider);
        /** @brief Clears the wrapped buffer and provider. */
        void Reset();

        /** @brief Enqueues the buffer to the waveOut device.
         *  @return True if the buffer was accepted by the device. */
        bool WriteToOutput();

        /** @brief Returns the wrapped buffer's provider.
         *  @return The provider, or null. */
        IBufferProvider* GetBufferProvider() const;
        /** @brief Returns the wrapped PCM buffer.
         *  @return The buffer, or null. */
        IBuffer* GetWrappedBuffer() const;

    private:
        /** @brief Prepares the WAVEHDR for the wrapped buffer. */
        void Initialize();

        /** @brief The owning WaveOut output. */
        WaveOut *waveOut;
        /** @brief The wrapped PCM buffer. */
        IBuffer *buffer;
        /** @brief The provider owning the buffer. */
        IBufferProvider *provider;
        /** @brief The waveOut header describing the buffer. */
        WAVEHDR header;
        /** @brief Whether the buffer is in a reset state. */
        bool reset;
};
