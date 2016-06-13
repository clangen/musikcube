//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <core/config.h>
#include <boost/shared_ptr.hpp>
#include <core/sdk/IBuffer.h>

namespace musik { namespace core { namespace audio {

    class  Buffer;
    class  Stream;
    typedef std::shared_ptr<Buffer> BufferPtr;

    class Buffer : public IBuffer {
        private:
            Buffer();

        public:
            static BufferPtr Create();
            ~Buffer();

            virtual long SampleRate() const;
            virtual void SetSampleRate(long sampleRate);
            virtual int Channels() const;
            virtual void SetChannels(int channels);
            virtual float* BufferPointer() const;
            virtual long Samples() const;
            virtual void SetSamples(long samples);
            virtual long Bytes() const;
            virtual double Position() const;
            void SetPosition(double position);

            bool Append(BufferPtr appendBuffer);
            void CopyFormat(BufferPtr fromBuffer);

        private:
            void ResizeBuffer();

            float *buffer;
            long sampleSize;
            long internalBufferSize;
            long sampleRate;
            int channels;
            double position;
    };

} } }
