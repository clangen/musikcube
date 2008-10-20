//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, Björn Olievier
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
#include <core/filestreams/IFileStream.h>

//////////////////////////////////////////////////////////////////////////////
namespace musik { namespace core { namespace audio {
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////
///\brief
///IAudioSource is the main interface for decoders
//////////////////////////////////////////
class IAudioSource{
    protected: 
        virtual ~IAudioSource() {};

    public:    

        //////////////////////////////////////////
        ///\brief
        ///Destroy the object
        ///
        ///The Destroy method is used so that it's guaranteed that the object is 
        ///destroyed inside the right DLL/exe
        //////////////////////////////////////////
        virtual void    Destroy() = 0;

        //////////////////////////////////////////
        ///\brief
        ///Get the length of the source in milliseconds
        ///
        ///\param MS
        ///Pointer to millisecond to set 
        ///
        ///\returns
        ///true is set successfully
        //////////////////////////////////////////
        virtual bool    GetLength(unsigned long * MS) = 0;

        //////////////////////////////////////////
        ///\brief
        ///Set the position in the source (in milliseconds)
        ///
        ///\param MS
        ///Pointer to the position to set. This will be set to the actual position set on return.
        ///
        ///\returns
        ///true on success
        //////////////////////////////////////////
        virtual bool    SetPosition(unsigned long * MS,unsigned long totalMS) = 0;

        //////////////////////////////////////////
        ///\brief
        ///TODO: What does this one do?
        //////////////////////////////////////////
        virtual bool    SetState(unsigned long State) = 0;

        //////////////////////////////////////////
        ///\brief
        ///Get samplerate and number of channels
        ///
        ///\param SampleRate
        ///Samplerate to set
        ///
        ///\param Channels
        ///Channels to set
        ///
        ///\returns
        ///true on success
        //////////////////////////////////////////
        virtual bool    GetFormat(unsigned long * SampleRate, unsigned long * Channels) = 0;

        //////////////////////////////////////////
        ///\brief
        ///Get next buffer (decoded data)
        ///
        ///\param ppBuffer
        ///Pointer to returned buffer
        ///
        ///\param NumSamples
        ///Number of samples in the buffer (not entirely correct, it's samples*channels)
        ///TODO: Rename to better name.
        ///
        ///\returns
        ///false is there is nothing left to read
        ///
        ///The acctual bytes written to the buffer will be NumSamples*channels*sizeof(float)
        //////////////////////////////////////////
        virtual bool    GetBuffer(float ** ppBuffer, unsigned long * NumSamples) = 0; // return false to signal that we are done decoding.

        //////////////////////////////////////////
        ///\brief
        ///Open the stream
        ///
        ///\param fileStream
        ///pointer to the filestream object.
        ///
        ///\returns
        ///True if successfully opened
        //////////////////////////////////////////
        virtual bool    Open(musik::core::filestreams::IFileStream *fileStream) = 0;

};

//////////////////////////////////////////////////////////////////////////////

class IAudioSourceSupplier{
    protected: 
        virtual ~IAudioSourceSupplier() {};

    public: 
        virtual IAudioSource*   CreateAudioSource() = 0;
        virtual void            Destroy() = 0;
        virtual bool            CanHandle(const utfchar* extension) const = 0;

};

//////////////////////////////////////////////////////////////////////////////
}}} // NS
//////////////////////////////////////////////////////////////////////////////
