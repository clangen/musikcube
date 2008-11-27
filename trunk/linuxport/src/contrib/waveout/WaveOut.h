#pragma once

#include "pch.h"

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/thread.hpp> 

#include <core/audio/IAudioCallback.h>
#include <core/audio/IAudioOutput.h>

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
//TODO: decide how to set this when integrating with mC2
public: unsigned long GetOutputDevice() const   { return WAVE_MAPPER; }; 
public: unsigned long GetBufferSizeMs() const   { return 1500;         };

private: unsigned long ThreadProc(void);

private: WAVEFORMATPCMEX    m_waveFormatPCMEx;
private: HWAVEOUT           m_waveHandle;
private: WAVEHDR*           m_Buffers;

private: float*             m_pfAudioBuffer;
private: unsigned long      m_dwBufferSize;

private: bool               m_Playing;

//private: unsigned long      m_BufferLengthMS;
private: unsigned long      m_BlockSize;
private: unsigned long      m_Interval;
private: unsigned long      m_NumBuffers;

private: unsigned long      m_ActiveBuffer;
private: unsigned long      m_QueuedBuffers;
private: unsigned long      m_LastPlayedBuffer;

private: __int64            m_dwSamplesOut;

private: LARGE_INTEGER      m_liCountsPerSecond;
private: LARGE_INTEGER      m_liLastPerformanceCount;
private: unsigned long      m_dwLastTickCount;

private: IAudioCallback*    m_pCallback;

private: boost::mutex       audioMutex;
public:  boost::condition   audioCondition; // TODO: review access level
private: boost::thread*     audioThread;
private: bool               m_bThreadRun;
};

class WaveOutSupplier : public IAudioOutputSupplier
{
public: IAudioOutput* CreateAudioOutput() { return new WaveOut(); };
public: void          Destroy()           { delete this; };
};