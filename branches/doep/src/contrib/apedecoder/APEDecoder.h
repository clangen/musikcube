#pragma once

#include <core/audio/IAudioSource.h>

#include "All.h"
#include "MACLib.h"
#include "MACIO.h"

using namespace musik::core::audio;

#define MACFILTERPACKETSIZE 512 // in blocks (MAClib.h defines block as (n*sample), where n = number of channels)
#define QUANTFACTOR 4.656613428e-10F // used for 24-bit int -> 32-bit float conversion

class APEDecoder :	public IAudioSource
{
protected:
	float				m_Buffer[4096];
	IAPEDecompress*		MACDecompressor;

public:
    APEDecoder();
    ~APEDecoder();

	bool		Open(const utfchar* source);
	bool        Close();
	void		Destroy();
	bool		GetFormat(unsigned long * SampleRate, unsigned long * Channels);
	bool		GetLength(unsigned long * MS);
	bool		SetPosition(unsigned long * MS);
	bool		SetState(unsigned long State);
	bool		GetBuffer(float ** ppBuffer, unsigned long * NumSamples);

    const utfchar* GetSource() const { return sourcePath.c_str(); };

private:
    utfstring sourcePath;
};
