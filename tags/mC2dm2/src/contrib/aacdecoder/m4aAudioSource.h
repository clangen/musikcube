#pragma once
#include "iaudiosource.h"
#include "neaacdec.h"
#include "mp4ff.h"

using namespace musik::core::audio;

// TODO: clean up the entire decoder.  Possibly move libfaad to separate project
class M4ADecoder : public IAudioSource
{
protected:

	NeAACDecHandle				m_hDecoder;
    mp4ff_t				*		m_mp4file;
    FILE				*		m_mp4FileHandle;
	mp4ff_callback_t			m_mp4cb;
    int							m_mp4track;
    long						m_numSamples;
    long						m_sampleId;

	unsigned long				m_SampleRate;
	unsigned char				m_NumChannels;

public:

	M4ADecoder(void);
	~M4ADecoder(void);

	bool    Open(const utfchar* source);
	bool    Close();
	void    Destroy(void);
	bool    GetLength(unsigned long * MS);
	bool    SetPosition(unsigned long * MS);
	bool    SetState(unsigned long State);
	bool    GetFormat(unsigned long * SampleRate, unsigned long * Channels);
	bool    GetBuffer(float ** ppBuffer, unsigned long * NumSamples);

    const utfchar* GetSource() const { return sourcePath.c_str(); };

private:
    utfstring sourcePath;  
};
