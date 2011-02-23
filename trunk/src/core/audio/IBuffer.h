//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, Daniel Önnerby
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

//////////////////////////////////////////////////////////////////////////////
namespace musik { namespace core { namespace audio {
//////////////////////////////////////////////////////////////////////////////

class MUSIK_EXPORT IBuffer {
    public:

        //////////////////////////////////////////
        ///\brief
        ///Get the samplerate of the buffer
        //////////////////////////////////////////
        virtual long SampleRate() const = 0; 

        //////////////////////////////////////////
        ///\brief
        ///Set the buffers samplerate
        //////////////////////////////////////////
        virtual void SetSampleRate(long sampleRate) = 0; 

        //////////////////////////////////////////
        ///\brief
        ///Get the number of channels of the buffer
        //////////////////////////////////////////
        virtual int Channels() const = 0; 

        //////////////////////////////////////////
        ///\brief
        ///Set the number of channels of the buffer
        //////////////////////////////////////////
        virtual void SetChannels(int channels) = 0; 

        //////////////////////////////////////////
        ///\brief
        ///Get the pointer to the real buffer.
        ///
        ///The pointer may change when you set any of the buffers
        ///properties like samplerate, samples and channels
        //////////////////////////////////////////
        virtual float* BufferPointer() const = 0; 

        //////////////////////////////////////////
        ///\brief
        ///Get the number of samples in the buffer
        ///
        ///To clairify, one sample = one sample for each channel
        ///and that means that one sample = sizeof(float)*channels bytes big
        //////////////////////////////////////////
        virtual long Samples() const = 0;

        //////////////////////////////////////////
        ///\brief
        ///Set the number of samples in the buffer
        //////////////////////////////////////////
        virtual void SetSamples(long samples) = 0; 

        //////////////////////////////////////////
        ///\brief
        ///How many bytes does this object take
        //////////////////////////////////////////
        virtual long Bytes() const = 0;
};

//////////////////////////////////////////////////////////////////////////////
} } }
//////////////////////////////////////////////////////////////////////////////
