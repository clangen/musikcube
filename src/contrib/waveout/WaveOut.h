#pragma once

#include "pch.h"

#include "CriticalSection.h"

#include "core/audio/IAudioCallback.h"
#include "core/audio/IAudioOutput.h"

using namespace musik::core::audio;

class WaveOut : public IAudioOutput
{
public:	WaveOut(void);
private: ~WaveOut(void);

public: static void CALLBACK WaveCallback(HWAVEOUT hWave, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD dw2);

public: bool Open(void);
public: bool Close(void);
public: bool Initialize(void);
public: bool Shutdown(void);

public: void Destroy();

public: void SetCallback(IAudioCallback* pCallback) { m_pCallback = pCallback;	}
public: bool SetFormat(unsigned long SampleRate, unsigned long Channels);

public: bool Start(void);
public: bool Stop(void);
public: bool Reset(void);

public: unsigned long GetSampleRate()   const   { return m_waveFormatPCMEx.Format.nSamplesPerSec; }
public: unsigned long GetChannels()     const   { return m_waveFormatPCMEx.Format.nChannels; }
public: unsigned long GetBlockSize()    const   { return m_BlockSize; }

private: unsigned long GetInterval()    const   { return m_Interval; }
private: unsigned long GetMaxRewind()   const   { return m_NumBuffers; }

private: unsigned long GetMSOut() const;
private: __int64 GetSamplesOut()  const;

private: bool GetVisData(float* ToHere, unsigned long ulNumSamples) const;

//TODO: decide how to set this when integrating with mC2
public: int GetOutputDevice() { return WAVE_MAPPER; }; 

private: static unsigned long __stdcall		ThreadStub(void * in);
private: unsigned long						ThreadProc(void);

private: WAVEFORMATPCMEX    m_waveFormatPCMEx;
private: HWAVEOUT           m_waveHandle;
private: WAVEHDR*           m_Buffers;

private: float*             m_pfAudioBuffer;
private: unsigned long      m_dwBufferSize;

private: bool               m_Playing;

private: unsigned long      m_BufferLengthMS;
private: unsigned long      m_BlockSize;
private: unsigned long      m_Interval;
private: unsigned long      m_NumBuffers;

private: unsigned long      m_ActiveBuffer;  // Looks like a circular buffer
private: unsigned long      m_QueuedBuffers;
private: unsigned long      m_LastPlayedBuffer;

private: __int64            m_dwSamplesOut;

private: LARGE_INTEGER      m_liCountsPerSecond;
private: LARGE_INTEGER      m_liLastPerformanceCount;
private: unsigned long      m_dwLastTickCount;

private: IAudioCallback*    m_pCallback;

private: CriticalSection    m_AudioLock;
private: HANDLE             m_hEvent;

private: HANDLE             m_hThread;
private: unsigned long      m_dwThreadID;
private: bool               m_bThreadRun;

private: static const int   bufferSizeMs = 300;
};

class WaveOutSupplier : public IAudioOutputSupplier
{
public: IAudioOutput* CreateAudioOutput() { return new WaveOut(); };
public: void          Destroy()           { delete this; };
};