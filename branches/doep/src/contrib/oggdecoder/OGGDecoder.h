#pragma once

#include "core/audio/IAudioSource.h"

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

using namespace musik::core::audio;

class OGGDecoder :	public IAudioSource
{
protected:  float           *m_Buffer;
protected:  OggVorbis_File  m_vf;
protected:  FILE            *m_file;
protected:  unsigned int    m_currentsection;

public: OGGDecoder();
public: ~OGGDecoder();

public: bool    Open(const utfchar* source);
public: bool    Close();
public: void    Destroy(void);
public: bool    GetFormat(unsigned long * SampleRate, unsigned long * Channels);
public: bool    GetLength(unsigned long * MS);
public: bool    SetPosition(unsigned long * MS);
public: bool    SetState(unsigned long State);
public: bool    GetBuffer(float ** ppBuffer, unsigned long * NumSamples);

public: const utfchar* GetSource() const { return sourcePath.c_str(); };

private: utfstring sourcePath;
};
