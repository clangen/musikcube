#pragma once

#include <core/audio/IAudioCallBack.h>
#include <core/audio/AudioPacketizer.h>
#include <core/audio/CriticalSection.h>

namespace musik { namespace core { namespace audio {

class IAudioSource;
class IAudioOutput;
class Transport;

class AudioStream : public IAudioCallback
{
// Cleaned up
public: enum PlayState          { PlayStateUnknown = -1, PlayStateStopped, PlayStatePlaying };
public: enum FadeState          { FadeStateNone, FadeStateIn, FadeStateOut };
public: enum AudioStreamEvent   { EventPlaybackStarted, EventPlaybackFinished, EventMixPointReached }; 

public: static const unsigned long UnknownLength = ULONG_MAX;

public:                     AudioStream(IAudioSource* source, IAudioOutput* output, Transport *owner);
public:                     ~AudioStream();

public:     bool            Start();
public:     bool            Stop();
public:     bool            SetVolumeScale(float scale);

public:     bool            GetBuffer(float * pAudioBuffer, unsigned long NumSamples); // IAudioCallback

private:    Transport*      transport; // Need this to trigger some signals
private:    IAudioSource*   audioSource;
private: 	IAudioOutput*   output;
private:    AudioPacketizer packetizer;

private:    PlayState       playState;
private:    FadeState       fadeState;

private:    unsigned long   samplesOut;

private:    float           volumeScale;    //0.0 - 1.0 affects total volume output!
private:    float           volume;
private:    float           volumeChange;

public:     bool            isFinished;

private:    bool            mixNotify;
private:    bool            isLast; // This can probably be removed once we have playlists

private:    unsigned long   channels;

private:    CriticalSection lock;

/////////////////////////////////////////
// Pending stuff

// Change modifiers as required.  Sticking to private untill really used outside of the class.
private:    unsigned long   GetLength()                     const;
private:    unsigned long   GetPosition()                   const;
private:    bool            SetPosition(unsigned long MS);

// TODO: decide what to do with this when integrating into mC2 
int GetCrossfadeTime() {return 0;}; // TuniacApp.Preferences
bool GetActivePlaylistCheckNext() { return false; }; //TuniacApp.PLaylistManager

////////////////////////////:
// Old - this isn't used anywhere.  Maybe we'll need it again.
//private:
//	unsigned long	GetState(void);
//	unsigned long	GetFadeState(void);
//	bool			IsFinished(void);
//	bool			FadeIn(unsigned long ulMS);
//	bool			FadeOut(unsigned long ulMS);
//	bool			GetVisData(float * ToHere, unsigned long ulNumSamples);
};

}}} // NS
