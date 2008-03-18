#pragma once

#include <core/audio/IAudioSource.h>

#include "ntddcdrm.h"
#include "devioctl.h"

using namespace musik::core::audio;

class CDDAAudioSource : public IAudioSource
{
protected:
	LONGLONG		m_llPosition, 
					m_llLength;

	HANDLE			m_hDrive;
	CDROM_TOC		m_TOC;
	UINT			m_nFirstSector, 
					m_nStartSector, 
					m_nStopSector;

	unsigned long	m_Channels;

	HRESULT Read(PBYTE pbBuffer, DWORD dwBytesToRead, BOOL bAlign, LPDWORD pdwBytesRead);


public:
	CDDAAudioSource(void);
	~CDDAAudioSource(void);

    bool    Open(const utfchar* source);
	void    Destroy(void);
	bool    GetLength(unsigned long * MS);
	bool    SetPosition(unsigned long * MS);
	bool    SetState(unsigned long State);
	bool    GetFormat(unsigned long * SampleRate, unsigned long * Channels);
	bool    GetBuffer(float ** ppBuffer, unsigned long * NumSamples);
};
