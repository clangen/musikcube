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

namespace musik { namespace core { namespace audio {

class IAudioCallback;

class IAudioOutput
{
public: virtual void Destroy() = 0;

private:  virtual bool  Open(void)          = 0;
private:  virtual bool  Close(void)         = 0;
private:  virtual bool  Initialize(void)    = 0;
private:  virtual bool  Shutdown(void)      = 0;

public: virtual void    SetCallback(IAudioCallback * pCallback)                     = 0;
public: virtual bool    SetFormat(unsigned long SampleRate, unsigned long Channels) = 0;

public: virtual bool    Start(void) = 0;
public: virtual bool    Stop(void)  = 0;
public: virtual bool    Reset(void) = 0;

public: virtual unsigned long   GetSampleRate()     const = 0;
public: virtual unsigned long   GetChannels()       const = 0;
public: virtual unsigned long   GetBlockSize()      const = 0;
public: virtual unsigned long   GetOutputDevice()   const = 0;
public: virtual unsigned long   GetBufferSizeMs()   const = 0;
};

class IAudioOutputSupplier
{
public: virtual void          Destroy()           = 0;
public: virtual IAudioOutput* CreateAudioOutput() = 0;
};
}}} // NS
