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

/** @file IEnvironment.h @brief Defines the IEnvironment interface for accessing application services and settings. */
#pragma once

#include "constants.h"
#include "IDataStream.h"
#include "IDecoder.h"
#include "IEncoder.h"
#include "IPreferences.h"
#include "IOutput.h"
#include "ITrackList.h"
#include "IDebug.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief The application's facade to plugins, exposing paths, codec
     *  factories, output devices, preferences, and playback settings. */
    class IEnvironment {
        public:
            /** @brief Retrieves a well-known application path.
             *  @param type The type of path to retrieve.
             *  @param dst The destination buffer for the path.
             *  @param size The capacity of the destination buffer.
             *  @return The number of bytes written, or the required size if the buffer was too small. */
            virtual size_t GetPath(PathType type, char* dst, int size) = 0;

            /** @brief Opens a data stream for the given URI.
             *  @param uri The URI to open.
             *  @param flags The access flags for the stream.
             *  @return The opened stream, or null on failure. */
            virtual IDataStream* GetDataStream(const char* uri, OpenFlags flags) = 0;

            /** @brief Creates a decoder for the given stream.
             *  @param stream The stream to decode.
             *  @return A decoder configured for the stream, or null if unsupported. */
            virtual IDecoder* GetDecoder(IDataStream* stream) = 0;

            /** @brief Creates an encoder for the given output format.
             *  @param type The encoder type, typically a file extension.
             *  @return An encoder instance, or null if unsupported. */
            virtual IEncoder* GetEncoder(const char* type) = 0;

            /** @brief Allocates an audio buffer of the given dimensions.
             *  @param samples The number of samples per channel.
             *  @param rate The sample rate in Hz.
             *  @param channels The number of channels.
             *  @return A newly allocated buffer. */
            virtual IBuffer* GetBuffer(size_t samples, size_t rate = 44100, size_t channels = 2) = 0;

            /** @brief Retrieves a named preferences object.
             *  @param name The name of the preferences scope.
             *  @return The preferences object. */
            virtual IPreferences* GetPreferences(const char* name) = 0;

            /** @brief Retrieves the application's logging interface.
             *  @return The IDebug instance. */
            virtual IDebug* GetDebug() = 0;

            /** @brief Returns the number of registered output plugins.
             *  @return The output count. */
            virtual size_t GetOutputCount() = 0;

            /** @brief Returns the output plugin at the given index.
             *  @param index The zero-based index.
             *  @return The output plugin, or null if out of range. */
            virtual IOutput* GetOutputAtIndex(size_t index) = 0;

            /** @brief Returns the output plugin with the given name.
             *  @param name The plugin name.
             *  @return The output plugin, or null if not found. */
            virtual IOutput* GetOutputWithName(const char* name) = 0;

            /** @brief Returns the current ReplayGain mode.
             *  @return The active ReplayGain mode. */
            virtual ReplayGainMode GetReplayGainMode() = 0;

            /** @brief Sets the ReplayGain mode.
             *  @param mode The mode to activate. */
            virtual void SetReplayGainMode(ReplayGainMode mode) = 0;

            /** @brief Returns the preamp gain applied before ReplayGain.
             *  @return The preamp gain, in dB. */
            virtual float GetPreampGain() = 0;

            /** @brief Sets the preamp gain applied before ReplayGain.
             *  @param gain The preamp gain, in dB. */
            virtual void SetPreampGain(float gain) = 0;

            /** @brief Returns whether the equalizer is enabled.
             *  @return True if the equalizer is active. */
            virtual bool GetEqualizerEnabled() = 0;

            /** @brief Enables or disables the equalizer.
             *  @param enabled True to enable the equalizer. */
            virtual void SetEqualizerEnabled(bool enabled) = 0;

            /** @brief Copies the current equalizer band values into the given array.
             *  @param target The destination array for the band values.
             *  @param count The capacity of the destination array.
             *  @return True if the values were retrieved. */
            virtual bool GetEqualizerBandValues(double target[], size_t count) = 0;

            /** @brief Sets the equalizer band values.
             *  @param values The band values to apply.
             *  @param count The number of values provided.
             *  @return True if the values were applied. */
            virtual bool SetEqualizerBandValues(double values[], size_t count) = 0;

            /** @brief Reloads the active playback output plugin. */
            virtual void ReloadPlaybackOutput() = 0;

            /** @brief Sets the default playback output plugin.
             *  @param output The output to use as the default. */
            virtual void SetDefaultOutput(IOutput* output) = 0;

            /** @brief Returns the default playback output plugin.
             *  @return The default output. */
            virtual IOutput* GetDefaultOutput() = 0;

            /** @brief Returns the active transport type.
             *  @return The transport type. */
            virtual TransportType GetTransportType() = 0;

            /** @brief Sets the active transport type.
             *  @param type The transport type to use. */
            virtual void SetTransportType(TransportType type) = 0;

            /** @brief Requests that metadata be reindexed for all sources. */
            virtual void ReindexMetadata() = 0;

            /** @brief Requests a full metadata rebuild. */
            virtual void RebuildMetadata() = 0;

            /** @brief Returns the application version string.
             *  @return The application version. */
            virtual const char* GetAppVersion() = 0;
    };

} } }
