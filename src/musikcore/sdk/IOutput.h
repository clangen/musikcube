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

/** @file IOutput.h @brief Defines the IOutput interface implemented by audio output plugins. */
#pragma once

#include "constants.h"
#include "IPlugin.h"
#include "IDataStream.h"
#include "IBuffer.h"
#include "IBufferProvider.h"
#include "IDevice.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief An audio output plugin that plays decoded PCM buffers on a
     *  physical or virtual audio device. */
    class IOutput {
        public:
            /** @brief Releases the output; callers must invoke this when done. */
            virtual void Release() = 0;

            /** @brief Pauses playback. */
            virtual void Pause() = 0;

            /** @brief Resumes playback after a pause. */
            virtual void Resume() = 0;

            /** @brief Sets the output volume.
             *  @param volume The volume, from 0.0 to 1.0. */
            virtual void SetVolume(double volume) = 0;

            /** @brief Returns the current output volume.
             *  @return The volume, from 0.0 to 1.0. */
            virtual double GetVolume() = 0;

            /** @brief Stops playback and releases buffered audio. */
            virtual void Stop() = 0;

            /** @brief Submits a buffer of audio for playback.
             *  @param buffer The PCM audio buffer to play.
             *  @param provider The callback to invoke once the buffer has been consumed.
             *  @return The result of submitting the buffer. */
            virtual OutputState Play(IBuffer *buffer, IBufferProvider *provider) = 0;

            /** @brief Blocks until all queued audio has finished playing. */
            virtual void Drain() = 0;

            /** @brief Returns the output's latency, in milliseconds.
             *  @return The latency in milliseconds. */
            virtual double Latency() = 0;

            /** @brief Returns the name of the output plugin.
             *  @return The plugin name. */
            virtual const char* Name() = 0;

            /** @brief Returns the default sample rate used by the output.
             *  @return The sample rate in Hz. */
            virtual int GetDefaultSampleRate() = 0;

            /** @brief Returns the list of devices exposed by this output.
             *  @return The device list. */
            virtual IDeviceList* GetDeviceList() = 0;

            /** @brief Sets the default device by id.
             *  @param deviceId The id of the device to use.
             *  @return True if the device was selected. */
            virtual bool SetDefaultDevice(const char* deviceId) = 0;

            /** @brief Returns the default device.
             *  @return The default device. */
            virtual IDevice* GetDefaultDevice() = 0;
    };

} } }
