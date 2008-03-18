#include "StdAfx.h"
#include <math.h>
#include "mp3decoder.h"

MP3Decoder::MP3Decoder() :
	m_hFile(NULL),
	m_pDecoder(NULL)
{
}

MP3Decoder::~MP3Decoder(void)
{
	Close();
}

unsigned long MP3Decoder::GetID3HeaderLength(unsigned char * buffer)
{
	unsigned char VerMajor;
	unsigned char VerMinor;
	unsigned char Flags;
	unsigned long Length;

	if( (toupper(buffer[0]) == 'I') &&
		(toupper(buffer[1]) == 'D') &&
		(toupper(buffer[2]) == '3') )
	{
		VerMajor = buffer[3];
		VerMinor = buffer[4];
		Flags = buffer[5];

		Length = (buffer[6] << 21) | (buffer[7] << 14) | (buffer[8] << 7) | buffer[9];
		Length += 10; // the header

		return(Length);
	}

	return(0);
}

bool MP3Decoder::GetXingHeader(unsigned char * XingBuffer)
{
#define FRAMES_FLAG     0x0001
#define BYTES_FLAG      0x0002
#define TOC_FLAG        0x0004
#define VBR_SCALE_FLAG  0x0008
#define GET_INT32BE(b) (i = (b[0] << 24) | (b[1] << 16) | b[2] << 8 | b[3], b += 4, i)

	unsigned long i;

	m_bXingValid = false;

	if(strncmp((char *)XingBuffer, "Xing", 4))
		if(strncmp((char *)XingBuffer, "Info", 4))
			return false;

	XingBuffer += 4;

	unsigned long HeadFlags = GET_INT32BE(XingBuffer);

	m_NumFrames = 0;
	if(HeadFlags & FRAMES_FLAG)
		m_NumFrames = GET_INT32BE(XingBuffer);
	if(m_NumFrames < 1)
		return false;

	m_StreamDataLength = 0;
	if(HeadFlags & BYTES_FLAG)
		m_StreamDataLength = GET_INT32BE(XingBuffer);

	if(HeadFlags & TOC_FLAG)
	{
		for (i = 0; i < 100; i++)
			m_TOC[i] = XingBuffer[i];
		XingBuffer += 100;		
	}
		
	m_VbrScale = -1;
	if(HeadFlags & VBR_SCALE_FLAG)
	{
		m_VbrScale = GET_INT32BE(XingBuffer);
	}

	m_bXingValid = true;

	return true;
}

bool MP3Decoder::GetStreamData(void)
{
	unsigned char tbuf[11];
	unsigned long bytesread;
	Frame fr;

	ReadFile(m_hFile, tbuf, 10, &bytesread, NULL);

	m_ID3v2Length = GetID3HeaderLength(tbuf);
	SetFilePointer(m_hFile, m_ID3v2Length, NULL, FILE_BEGIN);

	if(m_Splitter.Process(m_hFile, fr))
	{
		unsigned char * pHeader = fr.m_Data;
		if(!GetXingHeader(pHeader))
		{
			// analyse file here
			SetFilePointer(m_hFile, m_ID3v2Length, NULL, FILE_BEGIN);
			// just guesstimate the number of frames!

			m_StreamDataLength = GetFileSize(m_hFile, NULL) - m_ID3v2Length;
			// TODO: check for ID3 TAG at the end of the file and subtract
			// also remove the size of this current header
			m_StreamDataLength -= fr.m_Header.GetTotalFrameSize();

			m_NumFrames = m_StreamDataLength / fr.m_Header.GetTotalFrameSize(); 
		}
		else
		{
			if(m_bXingValid == false)
				LogConsoleMessage(TEXT("MP3 Decoder"), TEXT("Mp3 has Xing header but it is invalid."));
		}

		double bs[3] = { 384.0, 1152.0, 1152.0 };
		double TimePerFrame = (double)bs[fr.m_Header.GetLayer()] / (((double)fr.m_Header.GetSampleFrequency() / 1000.0));

		m_StreamLengthMS = TimePerFrame * m_NumFrames;

		if(fr.m_Header.GetMpegVersion() != MPEG1)
			m_StreamLengthMS /= 2;

		m_SampleRate	= fr.m_Header.GetSampleFrequency();
		m_NumChannels	= fr.m_Header.GetChannels();

		return true;
	}

	LogConsoleMessage(TEXT("MP3 Decoder"), TEXT("Error calculating mp3 stream information."));

	return false;
}

bool MP3Decoder::Open(const utfchar* sourcePath)
{
	m_LastLayer = -1;

    m_hFile = CreateFile(	sourcePath,
							GENERIC_READ, 
							FILE_SHARE_READ | FILE_SHARE_WRITE,
							NULL,
							OPEN_EXISTING,
							FILE_FLAG_SEQUENTIAL_SCAN,
							NULL);

	if(m_hFile == INVALID_HANDLE_VALUE)
	{
		m_hFile = NULL;
		return false;
	}

	return GetStreamData();
}

bool MP3Decoder::Close(void)
{
	if(m_pDecoder)
	{
		delete m_pDecoder;
		m_pDecoder = NULL;
	}

	if(m_hFile)
	{
		CloseHandle(m_hFile);
		m_hFile = NULL;
	}

	return false;
}

void		MP3Decoder::Destroy(void)
{
	delete this;
}

bool		MP3Decoder::GetFormat(unsigned long * SampleRate, unsigned long * Channels)
{
	*SampleRate	= m_SampleRate;
	*Channels	= m_NumChannels;

	return true;
}

bool		MP3Decoder::SetState(unsigned long State)
{
	return true;
}

bool		MP3Decoder::GetBuffer(float ** ppBuffer, unsigned long * NumSamples)
{
	static float SampleBuffer[2304];

	if(m_Splitter.Process(m_hFile, m_Frame))
	{
		if((m_Frame.m_Header.GetLayer() != m_LastLayer) || (m_pDecoder == NULL))
		{
			switch(m_Frame.m_Header.GetLayer())
			{
				case LAYER3:
					{
						if(m_pDecoder)
						{
							delete m_pDecoder;
							m_pDecoder = NULL;
						}
						m_pDecoder = new CLayer3Decoder();
						m_LastLayer = LAYER3;
					}
					break;

				default:
					LogConsoleMessage(TEXT("MP3 Decoder"), TEXT("Unsupported Layer (Only Layer 3 supported)."));
					return false;
					break;
			}
		}

		if(!m_pDecoder->ProcessFrame(&m_Frame, SampleBuffer, NumSamples))
		{
			*ppBuffer	= NULL;
			*NumSamples = 0;

			m_Frame.m_Header.Reset();
			return true;
		}

		*ppBuffer = SampleBuffer;

		return(true);
	}

	return false;
}

bool		MP3Decoder::GetLength(unsigned long * MS)
{
	*MS = m_StreamLengthMS;
	return true;
}

bool		MP3Decoder::SetPosition(unsigned long * MS)
{
	unsigned long newMS = *MS;
	float percent = 100.00f * ((float)newMS / (float)m_StreamLengthMS);
	unsigned long offset;

	if(m_bXingValid)
	{
		/* interpolate in TOC to get file seek point in bytes */ 
		int a;
		float fa, fb, fx;

		a = min(percent, 99);
		
		fa = m_TOC[a];
		
		if (a < 99)
			fb = m_TOC[a + 1];
		else
			fb = 256;
		
		fx = fa + (fb - fa) * (percent - a);
		offset = (1.0f / 256.0f) * fx * m_StreamDataLength;
	}
	else
	{
		offset = (float)m_StreamDataLength * (float)(percent/ 100.0f) ;
	}

	SetFilePointer(m_hFile, offset + m_ID3v2Length, NULL, FILE_BEGIN);
	
	m_Splitter.Process(m_hFile, m_Frame);
	if(m_pDecoder)
	{
		delete m_pDecoder;
		m_pDecoder = NULL;
	}

	return true;
}
