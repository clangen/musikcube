#include "stdafx.h"
#include "cddaaudiosource.h"

#define RAW_SECTOR_SIZE 2352
#define MSF2UINT(hgs) ((hgs[1]*4500)+(hgs[2]*75)+(hgs[3]))

CDDAAudioSource::CDDAAudioSource(void)
{
	m_hDrive = INVALID_HANDLE_VALUE;

	m_llPosition = m_llLength = 0;

	memset(&m_TOC, 0, sizeof(m_TOC));
	m_nStartSector = m_nStopSector = 0;

}

CDDAAudioSource::~CDDAAudioSource(void)
{
	if(m_hDrive != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hDrive);
		m_hDrive = INVALID_HANDLE_VALUE;
	}
}

bool		CDDAAudioSource::Open(const utfchar* source)
{
	TCHAR drive[32];
	int iTrackIndex;

	StrCpy(	drive, TEXT("\\\\.\\"));
    StrCatN(drive, &source[8], 2);
	StrCat(	drive, TEXT(":"));

	m_hDrive = CreateFile(	drive, 
							GENERIC_READ, 
							FILE_SHARE_READ, 
							NULL, 
							OPEN_EXISTING, 
							FILE_ATTRIBUTE_READONLY|FILE_FLAG_SEQUENTIAL_SCAN, 
							(HANDLE)NULL);

	if(m_hDrive == INVALID_HANDLE_VALUE)
	{
		return(false);
	}

    iTrackIndex = _wtoi(&source[10]);

	DWORD BytesReturned;
	if(!DeviceIoControl(	m_hDrive, 
							IOCTL_CDROM_READ_TOC, 
							NULL, 
							0, 
							&m_TOC, 
							sizeof(m_TOC), 
							&BytesReturned, 
							0)
	|| !(m_TOC.FirstTrack <= iTrackIndex && iTrackIndex <= m_TOC.LastTrack))
	{
		CloseHandle(m_hDrive);
		m_hDrive = INVALID_HANDLE_VALUE;
		return(false);
	}

	// MMC-3 Draft Revision 10g: Table 222 – Q Sub-channel control field
	m_TOC.TrackData[iTrackIndex-1].Control &= 5;
	if(!(m_TOC.TrackData[iTrackIndex-1].Control == 0 || m_TOC.TrackData[iTrackIndex-1].Control == 1))
	{
		CloseHandle(m_hDrive);
		m_hDrive = INVALID_HANDLE_VALUE;
		return(false);
	}

	m_Channels = 2;
	if(m_TOC.TrackData[iTrackIndex-1].Control&8) 
		m_Channels = 4;

	m_nStartSector = MSF2UINT(m_TOC.TrackData[iTrackIndex-1].Address) - 150;//MSF2UINT(m_TOC.TrackData[0].Address);
	m_nStopSector = MSF2UINT(m_TOC.TrackData[iTrackIndex].Address) - 150;//MSF2UINT(m_TOC.TrackData[0].Address);

	m_llLength = (m_nStopSector-m_nStartSector)*RAW_SECTOR_SIZE;

	return true;
}


void		CDDAAudioSource::Destroy(void)
{
	delete this;
}

bool		CDDAAudioSource::GetLength(unsigned long * MS)
{
	*MS = (unsigned long)(((float)m_llLength / (float)((44100 * 2) * m_Channels)) * 1000.0f);
	return true;
}

bool		CDDAAudioSource::SetPosition(unsigned long * MS)
{
	m_llPosition = (((float)((44100 * 2) * m_Channels)) / 1000.0f) * (*MS);

	while(m_llPosition % (2 * m_Channels))
		m_llPosition ++;
	return true;
}

bool		CDDAAudioSource::SetState(unsigned long State)
{
	return true;
}

bool		CDDAAudioSource::GetFormat(unsigned long * SampleRate, unsigned long * Channels)
{
	*SampleRate		= 44100;
	*Channels		= m_Channels;
	return true;
}

HRESULT CDDAAudioSource::Read(PBYTE pbBuffer, DWORD dwBytesToRead, BOOL bAlign, LPDWORD pdwBytesRead)
{
	BYTE buff[RAW_SECTOR_SIZE];

	PBYTE pbBufferOrg = pbBuffer;
	LONGLONG pos = m_llPosition;
	size_t len = (size_t)dwBytesToRead;

	while(pos >= 0 && pos < m_llLength && len > 0)
	{
		RAW_READ_INFO rawreadinfo;
		rawreadinfo.SectorCount = 1;
		rawreadinfo.TrackMode = CDDA;

		UINT sector = m_nStartSector + int(pos/RAW_SECTOR_SIZE);
		__int64 offset = pos%RAW_SECTOR_SIZE;

		rawreadinfo.DiskOffset.QuadPart = sector*2048;
		DWORD BytesReturned = 0;
		BOOL b = DeviceIoControl(
					m_hDrive, IOCTL_CDROM_RAW_READ,
					&rawreadinfo, sizeof(rawreadinfo),
					buff, RAW_SECTOR_SIZE,
					&BytesReturned, 0);

		size_t l = (size_t)min(min(len, RAW_SECTOR_SIZE - offset), m_llLength - pos);
		memcpy(pbBuffer, &buff[offset], l);

		pbBuffer += l;
		pos += l;
		len -= l;
	}

	if(pdwBytesRead) *pdwBytesRead = pbBuffer - pbBufferOrg;
	m_llPosition += pbBuffer - pbBufferOrg;

	return S_OK;
}

bool		CDDAAudioSource::GetBuffer(float ** ppBuffer, unsigned long * NumSamples)
{
#define BUFSIZE	4096
	static float	audiobuffer[BUFSIZE];
	static BYTE		buff[BUFSIZE];

	if(Read(buff, BUFSIZE, TRUE, NULL) == S_OK)
	{
		short * t = (short*)buff;

		for(int x=0; x<(BUFSIZE/2); x++)
		{
			audiobuffer[x] = (float)t[x] / 16384.0f;
		}
	}

	*ppBuffer	= audiobuffer;
	*NumSamples =(BUFSIZE/2);


	return true;
}



