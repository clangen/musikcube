#pragma once

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>

#include <core/audio/AudioPacketizer.h>
#include <core/audio/IAudioCallBack.h>

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

private:    boost::mutex    mutex;

private: static unsigned long streamsCreated;
private: unsigned long streamId;

public: unsigned long GetStreamId() const { return this->streamId; };
public: utfstring ToString() const;

public:    unsigned long   GetLength()                     const;
public:    unsigned long   GetPosition()                   const;
public:    bool            SetPosition(unsigned long MS);

/////////////////////////////////////////
// Pending stuff


// TODO: decide what to do with this when integrating into mC2 
int GetCrossfadeTime() {return 0;}; // TuniacApp.Preferences
bool GetActivePlaylistCheckNext() { return false; }; //TuniacApp.PLaylistManager
};

}}} // NS
