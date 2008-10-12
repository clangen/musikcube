#include "WaveOut.h"

#include <boost/bind.hpp>

WaveOut::WaveOut(void) 
 :m_waveHandle(NULL)
 ,m_pCallback(NULL)
 ,audioThread(NULL)
 ,m_dwSamplesOut(0)
 ,m_LastPlayedBuffer(-1)
 ,m_NumBuffers(8) //TODO: config
 ,m_BlockSize(0)
 ,m_pfAudioBuffer(NULL)
{
	ZeroMemory(&m_waveFormatPCMEx, sizeof(m_waveFormatPCMEx));

	QueryPerformanceFrequency(&m_liCountsPerSecond);

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

        to->audioCondition.notify_one();
   }
}

bool WaveOut::Open(void)
{
	int r;
	int currentDevice = this->GetOutputDevice();

    boost::mutex::scoped_lock lock(this->audioMutex);

	if(m_waveHandle==NULL)
	{
		r = waveOutOpen(&m_waveHandle, currentDevice, (WAVEFORMATEX*)&m_waveFormatPCMEx, (DWORD_PTR)WaveCallback, (DWORD_PTR)this, CALLBACK_FUNCTION);
		if(r != MMSYSERR_NOERROR) 
		{
			return(false);
		}

		m_Buffers = (WAVEHDR *)VirtualAlloc(NULL, m_NumBuffers * sizeof(WAVEHDR), MEM_COMMIT, PAGE_READWRITE);

        unsigned int bytesPerSample = m_waveFormatPCMEx.Format.wBitsPerSample/8;

        m_dwBufferSize = m_BlockSize*bytesPerSample;

		m_pfAudioBuffer = (float *)VirtualAlloc(NULL, m_dwBufferSize * m_NumBuffers, MEM_COMMIT, PAGE_READWRITE);		// allocate audio memory
		VirtualLock(m_pfAudioBuffer, m_dwBufferSize * m_NumBuffers);													// lock the audio memory into physical memory

		for(unsigned long x=0; x<m_NumBuffers; x++)
		{
            m_Buffers[x].dwBufferLength		= m_dwBufferSize;
            m_Buffers[x].lpData				= (LPSTR)&(this->m_pfAudioBuffer[x*m_BlockSize]);
 			m_Buffers[x].dwUser				= x;
			m_Buffers[x].dwBytesRecorded	= 0;
			m_Buffers[x].dwFlags			= 0;
			m_Buffers[x].dwLoops			= 0;

			waveOutPrepareHeader(m_waveHandle, &m_Buffers[x], sizeof(WAVEHDR));
            m_Buffers[x].dwFlags			|= WHDR_DONE;
		}

		QueryPerformanceCounter(&m_liLastPerformanceCount);

	}

	return true;
}

bool WaveOut::Close(void)
{
	int r;

    boost::mutex::scoped_lock lock(this->audioMutex);

	if(m_waveHandle)
	{
		m_Playing = false;

        do{
		    if(waveOutReset(m_waveHandle) == MMSYSERR_NOERROR)
		    {
			    for(unsigned long x=0; x<m_NumBuffers; x++)
			    {
				    if(m_Buffers[x].dwFlags & WHDR_PREPARED)
					    waveOutUnprepareHeader(m_waveHandle, &m_Buffers[x], sizeof(WAVEHDR));
			    }
		    }
        } while((r = waveOutClose(m_waveHandle)) != MMSYSERR_NOERROR);


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

unsigned long WaveOut::ThreadProc(void)
{
    bool hasPlayed = false;

	while(m_bThreadRun && !hasPlayed)
	{
		while(m_Playing && this->m_pCallback)
		{
			boost::mutex::scoped_lock lock(this->audioMutex);

            while(this->m_Buffers && m_Playing && ((m_Buffers[m_ActiveBuffer].dwFlags&WHDR_DONE)==0) )
			{
                this->audioCondition.wait(lock);
			}

			if(m_Playing)
			{
				if(m_pCallback->GetBuffer((float*)m_Buffers[m_ActiveBuffer].lpData, m_BlockSize))
				{
					m_Buffers[m_ActiveBuffer].dwUser = m_ActiveBuffer;
                    waveOutWrite(m_waveHandle, &m_Buffers[m_ActiveBuffer], sizeof(WAVEHDR));
                    m_ActiveBuffer++;
					m_ActiveBuffer &= (m_NumBuffers-1);
				}
				else
				{
					m_Playing = false;
                    hasPlayed = true;
                    this->audioCondition.wait(lock);
				}
			}
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

	if(!Open())
		return(false);

    m_bThreadRun = true;


    this->audioThread = new boost::thread(boost::bind(&WaveOut::ThreadProc, this));

	if(!this->audioThread)
		return false;

	return(true);
}

bool WaveOut::Shutdown(void)
{
	Close();
     
//    boost::mutex::scoped_lock lock(this->audioMutex);

	if(this->audioThread)
	{
		m_bThreadRun = false;

        this->audioCondition.notify_one();
        this->audioThread->join();
        delete this->audioThread;
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

        m_Interval			= (this->GetBufferSizeMs() / m_NumBuffers) / 2;
		m_BlockSize			= (this->GetBufferSizeMs() / m_NumBuffers) * samplesperms;	// this should be a 300MS buffer size!
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
//	if(!Open())
//		return(false);
 
    boost::mutex::scoped_lock lock(this->audioMutex);

	while(m_QueuedBuffers) // Handles pause/play?
	{
		waveOutWrite(m_waveHandle, &m_Buffers[m_ActiveBuffer], sizeof(WAVEHDR));
		m_ActiveBuffer++;
		m_ActiveBuffer &= (m_NumBuffers-1); // Probably only works if m_Numbuffers is power of 2

		m_QueuedBuffers--;
	}

	m_Playing = true;
	
    this->audioCondition.notify_one();

	return true;
}

bool WaveOut::Stop(void)
{
    boost::mutex::scoped_lock lock(this->audioMutex);

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