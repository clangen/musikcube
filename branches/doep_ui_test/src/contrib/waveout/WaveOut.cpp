#include "WaveOut.h"

WaveOut::WaveOut(void) :
	m_waveHandle(NULL),
	m_pCallback(NULL),
	m_hThread(NULL),
	m_hEvent(NULL),
	m_dwSamplesOut(0),
	m_LastPlayedBuffer(-1),
	m_NumBuffers(4)
{
	ZeroMemory(&m_waveFormatPCMEx, sizeof(m_waveFormatPCMEx));

	QueryPerformanceFrequency(&m_liCountsPerSecond);

	m_pfAudioBuffer = NULL;
}

WaveOut::~WaveOut(void)
{
	Shutdown();
}

void WaveOut::Destroy()
{
    delete this;
}

//////////////////////////////////////////////////////////////////////////////
//
//	Actual Audio Output Stuff!
//
//

void CALLBACK WaveOut::WaveCallback(HWAVEOUT hWave, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD dw2)
{
    if(uMsg == WOM_DONE) 
	{
		WaveOut*	to			= (WaveOut *)dwUser;
		LPWAVEHDR	whr					= (LPWAVEHDR)dw1;

		to->m_dwSamplesOut		+= (whr->dwBufferLength / to->m_waveFormatPCMEx.Format.nBlockAlign);
		to->m_LastPlayedBuffer	= (unsigned long)whr->dwUser;

		QueryPerformanceCounter(&to->m_liLastPerformanceCount);

 		SetEvent(to->m_hEvent);
   }
}

bool WaveOut::Open(void)
{
	int r;
	int currentDevice = this->GetOutputDevice();

	AutoLock lock(&m_AudioLock);

	if(m_waveHandle==NULL)
	{
		r = waveOutOpen(&m_waveHandle, currentDevice, (WAVEFORMATEX*)&m_waveFormatPCMEx, (DWORD_PTR)WaveCallback, (DWORD_PTR)this, CALLBACK_FUNCTION);
		if(r != MMSYSERR_NOERROR) 
		{
			return(false);
		}

		m_Buffers = (WAVEHDR *)VirtualAlloc(NULL, m_NumBuffers * sizeof(WAVEHDR), MEM_COMMIT, PAGE_READWRITE);
		m_dwBufferSize = m_BlockSize * m_NumBuffers;

		m_pfAudioBuffer = (float *)VirtualAlloc(NULL, m_dwBufferSize * m_NumBuffers, MEM_COMMIT, PAGE_READWRITE);		// allocate audio memory
		VirtualLock(m_pfAudioBuffer, m_dwBufferSize * m_NumBuffers);													// lock the audio memory into physical memory

		for(unsigned long x=0; x<m_NumBuffers; x++)
		{
			m_Buffers[x].dwBufferLength		= m_BlockSize * m_NumBuffers;
			m_Buffers[x].lpData				= (LPSTR)&m_pfAudioBuffer[x * m_BlockSize];
 			m_Buffers[x].dwUser				= x;
			m_Buffers[x].dwBytesRecorded	= 0;
			m_Buffers[x].dwFlags			= 0;
			m_Buffers[x].dwLoops			= 0;

			waveOutPrepareHeader(m_waveHandle, &m_Buffers[x], sizeof(WAVEHDR));
		}

		QueryPerformanceCounter(&m_liLastPerformanceCount);

	}

	return true;
}

bool WaveOut::Close(void)
{
	int r;

	AutoLock lock(&m_AudioLock);

	if(m_waveHandle)
	{
		m_Playing = false;

again:
		if(waveOutReset(m_waveHandle) == MMSYSERR_NOERROR)
		{
			for(unsigned long x=0; x<m_NumBuffers; x++)
			{
				if(m_Buffers[x].dwFlags & WHDR_PREPARED)
					waveOutUnprepareHeader(m_waveHandle, &m_Buffers[x], sizeof(WAVEHDR));
			}
		}

		while((r = waveOutClose(m_waveHandle)) != MMSYSERR_NOERROR)
		{
			goto again;
		}

		m_waveHandle = NULL;

		VirtualUnlock(m_pfAudioBuffer, m_dwBufferSize * m_NumBuffers);
		VirtualFree(m_pfAudioBuffer, 0, MEM_RELEASE);
		m_pfAudioBuffer = NULL;

		VirtualFree(m_Buffers, 0, MEM_RELEASE);
		m_Buffers = NULL;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////
//
//


unsigned long WaveOut::ThreadStub(void * in)
{
	WaveOut * pAO = (WaveOut *)in;
	return(pAO->ThreadProc());
}

unsigned long WaveOut::ThreadProc(void)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	while(m_bThreadRun)
	{
		while(m_Playing && this->m_pCallback)
		{
			AutoLock lock(&m_AudioLock);

			while(	(m_Buffers[m_ActiveBuffer].dwFlags & WHDR_INQUEUE) && m_Playing )
			{
				if(WaitForSingleObject(m_hEvent, m_Interval) == WAIT_OBJECT_0)
				{
					ResetEvent(m_hEvent);
				}
			}

			if(m_Playing)
			{
				if(m_pCallback->GetBuffer((float*)m_Buffers[m_ActiveBuffer].lpData, m_BlockSize))
				{
					m_Buffers[m_ActiveBuffer].dwUser = m_ActiveBuffer;
					waveOutWrite(m_waveHandle, &m_Buffers[m_ActiveBuffer], sizeof(WAVEHDR));
					m_ActiveBuffer++;
					m_ActiveBuffer &= (m_NumBuffers-1);

					if(WaitForSingleObject(m_hEvent, m_Interval) == WAIT_OBJECT_0)
					{
						ResetEvent(m_hEvent);
					}
				}
				else
				{
					m_Playing = false;
				}
			}
		}

		if(WaitForSingleObject(m_hEvent, m_Interval) == WAIT_OBJECT_0)
		{
			ResetEvent(m_hEvent);
		}

	}

	return(0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Initialization and termination
//
//
//

bool WaveOut::Initialize(void)
{
	m_Playing = false;
	m_ActiveBuffer	= 0;
	m_QueuedBuffers	= 0;

	m_hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(!m_hEvent)
		return(false);

	m_bThreadRun = true;
	m_hThread = CreateThread(	NULL,
								16384,
								ThreadStub,
								this,
								0,
								&m_dwThreadID);

	if(!m_hThread)
		return false;

	return(true);
}

bool WaveOut::Shutdown(void)
{
	Close();

	AutoLock lock(&m_AudioLock);

	if(m_hThread)
	{
		m_bThreadRun = false;

		SetEvent(m_hEvent);

		if(WaitForSingleObject(m_hThread, 10000) == WAIT_TIMEOUT)
			TerminateThread(m_hThread, 0);

		CloseHandle(m_hThread);
		m_hThread = NULL;
	}

	if(m_hEvent)
	{
		CloseHandle(m_hEvent);
		m_hEvent = NULL;
	}

	return(true);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//		Setup Stuff, can only be called before initialize
//
//

bool WaveOut::SetFormat(unsigned long SampleRate, unsigned long Channels)
{
	if( (SampleRate != m_waveFormatPCMEx.Format.nSamplesPerSec) ||
		(Channels != m_waveFormatPCMEx.Format.nChannels) )
	{
		Shutdown();

		DWORD speakerconfig;

		if(Channels == 1)
		{
			speakerconfig = KSAUDIO_SPEAKER_MONO;
		}
		else if(Channels == 2)
		{
			speakerconfig = KSAUDIO_SPEAKER_STEREO;
		}
		else if(Channels == 4)
		{
			speakerconfig = KSAUDIO_SPEAKER_QUAD;
		}
		else if(Channels == 5)
		{
			speakerconfig = (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT  | SPEAKER_BACK_RIGHT);
		}
		else if(Channels == 6)
		{
			speakerconfig = KSAUDIO_SPEAKER_5POINT1;
		}
		else
		{
			speakerconfig = 0;
		}

		m_waveFormatPCMEx.Format.cbSize					= 22;
		m_waveFormatPCMEx.Format.wFormatTag				= WAVE_FORMAT_EXTENSIBLE;
		m_waveFormatPCMEx.Format.nChannels				= (WORD)Channels;
		m_waveFormatPCMEx.Format.nSamplesPerSec			= SampleRate;
		m_waveFormatPCMEx.Format.wBitsPerSample			= 32;
		m_waveFormatPCMEx.Format.nBlockAlign			= (m_waveFormatPCMEx.Format.wBitsPerSample/8) * m_waveFormatPCMEx.Format.nChannels;
		m_waveFormatPCMEx.Format.nAvgBytesPerSec		= ((m_waveFormatPCMEx.Format.wBitsPerSample/8) * m_waveFormatPCMEx.Format.nChannels) * m_waveFormatPCMEx.Format.nSamplesPerSec; //Compute using nBlkAlign * nSamp/Sec 

        // clangen: wValidBitsPerSample/wReserved/wSamplesPerBlock are a union,
        // so don't set wReserved or wSamplesPerBlock to 0 after assigning
        // wValidBitsPerSample. (Vista bug)
		m_waveFormatPCMEx.Samples.wValidBitsPerSample	= 32;
		
		m_waveFormatPCMEx.dwChannelMask					= speakerconfig; 
		m_waveFormatPCMEx.SubFormat						= KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

		unsigned long samplesperms = ((unsigned long)((float)m_waveFormatPCMEx.Format.nSamplesPerSec / 1000.0f)) * m_waveFormatPCMEx.Format.nChannels;

        m_Interval			= (WaveOut::bufferSizeMs / m_NumBuffers) / 2;
		m_BlockSize			= (WaveOut::bufferSizeMs / m_NumBuffers) * samplesperms;	// this should be a 300MS buffer size!
		while(m_BlockSize & m_waveFormatPCMEx.Format.nBlockAlign)
			m_BlockSize++;

		if(!Initialize())
			return false;
	}

	return true;
}


////////////////////////////////////////////////////////////////////////////////////////////
// PLAYBACK CONTROLS
//
//
bool WaveOut::Start(void)
{
	AutoLock lock(&m_AudioLock);

	if(!Open())
		return(false);

	while(m_QueuedBuffers) // Handles pause/play?
	{
		waveOutWrite(m_waveHandle, &m_Buffers[m_ActiveBuffer], sizeof(WAVEHDR));
		m_ActiveBuffer++;
		m_ActiveBuffer &= (m_NumBuffers-1); // Probably only works if m_Numbuffers is power of 2

		m_QueuedBuffers--;
	}

	m_Playing = true;
	SetEvent(m_hEvent);

	return true;
}

bool WaveOut::Stop(void)
{
    AutoLock lock(&m_AudioLock);

	m_QueuedBuffers = 0;
	m_Playing = false;

	if(m_waveHandle)
	{
		for(unsigned long buffer=0; buffer<m_NumBuffers; buffer++)
		{
			if(	!(m_Buffers[buffer].dwFlags & WHDR_INQUEUE) )
			{
				m_QueuedBuffers++;
			}
		}
		m_ActiveBuffer-= m_QueuedBuffers;
		m_ActiveBuffer &= (m_NumBuffers-1);
	}

	Close();

	return true;
}

bool WaveOut::Reset(void)
{
//	CAutoLock lock(&m_AudioLock);

	// simmilar to stop, except we dont rewind the stream
	if(m_waveHandle) 
	{
		waveOutReset(m_waveHandle);

		m_ActiveBuffer = 0;
		m_dwSamplesOut = 0;

		m_LastPlayedBuffer = 0;

		QueryPerformanceCounter(&m_liLastPerformanceCount);
		return(true);
	}
	return false;
}

unsigned long WaveOut::GetMSOut() const
{
    __int64 SamplesOut = GetSamplesOut();

    return( (unsigned long)((float)SamplesOut / ((float)m_waveFormatPCMEx.Format.nSamplesPerSec / 1000.0f)) );
}

__int64 WaveOut::GetSamplesOut() const
{
	double t;

	if(m_waveHandle == NULL)
	{
		t = 0;
	}
	else
	{
		LARGE_INTEGER ThisCount;
		QueryPerformanceCounter(&ThisCount);

		double val = (double)(ThisCount.QuadPart - m_liLastPerformanceCount.QuadPart);
		t =  (val / (double)(m_liCountsPerSecond.QuadPart / 1000.0));
	}

	unsigned long samplesperms = (unsigned long)((float)m_waveFormatPCMEx.Format.nSamplesPerSec / 1000.0f);
	unsigned long samplesdif = (unsigned long)(t * samplesperms);

	return(m_dwSamplesOut + samplesdif);
}

bool WaveOut::GetVisData(float * ToHere, unsigned long ulNumSamples) const
{
	if(m_waveHandle && (m_LastPlayedBuffer != -1))
	{
		unsigned long offset = (GetSamplesOut() & ((m_BlockSize/m_waveFormatPCMEx.Format.nChannels)-1))*m_waveFormatPCMEx.Format.nChannels;
		float * pBuffer		= (float *)m_Buffers[(m_LastPlayedBuffer+1) & (m_NumBuffers-1)].lpData;

		if( (offset + ulNumSamples) > m_BlockSize )
		{
			unsigned long SamplesThisBlock = (m_BlockSize - offset);
			unsigned long SamplesNextBlock = ulNumSamples - SamplesThisBlock;
			float * pNextBuffer = (float *)m_Buffers[(m_LastPlayedBuffer+2) & (m_NumBuffers-1)].lpData;

			CopyFloat(ToHere,						&pBuffer[offset],	SamplesThisBlock);
			CopyFloat(&ToHere[SamplesThisBlock],	pNextBuffer,		SamplesNextBlock);
		}
		else
		{
			CopyFloat(ToHere, &pBuffer[offset], ulNumSamples);
		}
		return true;
	}

	return false;
}