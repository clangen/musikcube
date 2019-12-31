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

#pragma once

#include "constants.h"
#include "IDataStream.h"
#include "IDecoder.h"
#include "IEncoder.h"
#include "IPreferences.h"
#include "IOutput.h"
#include "ITrackList.h"
#include "IDebug.h"

namespace musik { namespace core { namespace sdk {

    class IEnvironment {
        public:
            virtual size_t GetPath(PathType type, char* dst, int size) = 0;
            virtual IDataStream* GetDataStream(const char* uri, OpenFlags flags) = 0;
            virtual IDecoder* GetDecoder(IDataStream* stream) = 0;
            virtual IEncoder* GetEncoder(const char* type) = 0;
            virtual IBuffer* GetBuffer(size_t samples, size_t rate = 44100, size_t channels = 2) = 0;
            virtual IPreferences* GetPreferences(const char* name) = 0;
            virtual IDebug* GetDebug() = 0;
            virtual size_t GetOutputCount() = 0;
            virtual IOutput* GetOutputAtIndex(size_t index) = 0;
            virtual IOutput* GetOutputWithName(const char* name) = 0;
            virtual ReplayGainMode GetReplayGainMode() = 0;
            virtual void SetReplayGainMode(ReplayGainMode mode) = 0;
            virtual float GetPreampGain() = 0;
            virtual void SetPreampGain(float gain) = 0;
            virtual bool GetEqualizerEnabled() = 0;
            virtual void SetEqualizerEnabled(bool enabled) = 0;
            virtual bool GetEqualizerBandValues(double target[], size_t count) = 0;
            virtual bool SetEqualizerBandValues(double values[], size_t count) = 0;
            virtual void ReloadPlaybackOutput() = 0;
            virtual void SetDefaultOutput(IOutput* output) = 0;
            virtual IOutput* GetDefaultOutput() = 0;
            virtual TransportType GetTransportType() = 0;
            virtual void SetTransportType(TransportType type) = 0;
            virtual void ReindexMetadata() = 0;
            virtual void RebuildMetadata() = 0;
    };

} } }
