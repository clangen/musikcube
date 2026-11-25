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

#include <common/DeleteDefaults.h>

namespace musik { namespace core { namespace audio {

    class Stream;

    class Buffer {
        public:
            enum Flags {
                NoFlags = 0,
                ImmutableSize = 1,
                NoDelete = 2
            };

            MUSIK_DELETE_COPY_AND_MOVE(Buffer);

            Buffer(Flags flags = NoFlags) noexcept;
            Buffer(float* buffer, int samples) noexcept;

            ~Buffer();

            long SampleRate() const noexcept;
            void SetSampleRate(long sampleRate) noexcept;
            int Channels() const noexcept;
            void SetChannels(int channels) noexcept;
            float* BufferPointer() const noexcept;
            long Samples() const noexcept;
            void SetSamples(long samples);
            long Capacity() const noexcept;

            double Position() const noexcept;
            void SetPosition(double position) noexcept;
            void Copy(float const* buffer, long samples, long offset = 0);
            void CopyFormat(Buffer* fromBuffer) noexcept;

        private:
            float *buffer;
            long samples;
            long internalBufferSize;
            long sampleRate;
            int channels;
            double position;
            int flags;
    };

} } }
