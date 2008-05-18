#pragma once

#include <core/config.h>

namespace musik { namespace core { namespace audio {

class IAudioSource
{
protected: virtual ~IAudioSource() {};

public:	virtual void    Destroy() = 0;
public:	virtual bool    GetLength(unsigned long * MS) = 0;
public:	virtual bool    SetPosition(unsigned long * MS) = 0;// upon calling, *MS is the position to seek to, on return set it to the actual offset we seeked to
public:	virtual bool    SetState(unsigned long State) = 0;
public:	virtual bool    GetFormat(unsigned long * SampleRate, unsigned long * Channels) = 0;
public:	virtual bool    GetBuffer(float ** ppBuffer, unsigned long * NumSamples) = 0; // return false to signal that we are done decoding.
public: virtual bool    Open(const utfchar* source) = 0;

public: virtual const utfchar* GetSource() const = 0; // TODO: remove?
};

class IAudioSourceSupplier
{
protected: virtual ~IAudioSourceSupplier() {};

public: virtual IAudioSource*   CreateAudioSource() = 0;
public: virtual void            Destroy() = 0;
public: virtual bool            CanHandle(const utfchar* source) const = 0;
};

}}} // NS
