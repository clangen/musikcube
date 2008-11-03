#pragma once

#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include <core/Track.h>
#include <core/audio/AudioPacketizer.h>
#include <core/audio/IAudioCallBack.h>
#include <core/filestreams/Factory.h>

namespace musik { namespace core { namespace audio {

class IAudioSource;
class IAudioOutput;
class Transport;

class AudioStream : public IAudioCallback
{
public: 
    enum PlayState          { PlayStateUnknown = -1, PlayStateStopped, PlayStatePlaying, PlayStatePaused };
    enum FadeState          { FadeStateNone, FadeStateIn, FadeStateOut };
    enum AudioStreamEvent   { EventPlaybackStarted, EventPlaybackFinished, EventMixPointReached }; 

    static const unsigned long UnknownLength = ULONG_MAX;

    AudioStream(IAudioSource* source, IAudioOutput* output, Transport *owner, musik::core::TrackPtr track,musik::core::filestreams::FileStreamPtr fileStream);
    ~AudioStream();

    bool            Start();
    bool            Stop();
    bool            Pause();
    bool            Resume();

    bool            SetVolumeScale(float scale);

    bool            GetBuffer(float * pAudioBuffer, unsigned long NumSamples); // IAudioCallback

    unsigned long   LengthMs()      const;
    unsigned long   PositionMs()    const;
    bool            SetPositionMs(unsigned long ms);
    bool            isFinished;

    musik::core::TrackPtr   Track() const { return this->track; };

    unsigned long GetStreamId() const { return this->streamId; };
    utfstring ToString() const;

private:    
    Transport*      transport; // Need this to trigger some signals
    IAudioSource*   audioSource;
    IAudioOutput*   output;
    AudioPacketizer packetizer;

    musik::core::TrackPtr   track;
    musik::core::filestreams::FileStreamPtr fileStream;

    PlayState       playState;
    FadeState       fadeState;

    unsigned long   samplesOut;

    float           volumeScale;    //0.0 - 1.0 affects total volume output!
    float           volume;
    float           volumeChange;

    bool            mixNotify;
    bool            isLast; // This can probably be removed once we have playlists

    unsigned long   channels;

    boost::mutex        mutex;
    boost::condition    pauseCondition;

    static unsigned long streamsCreated;
    unsigned long streamId;

/////////////////////////////////////////
// Pending stuff


// TODO: decide what to do with this when integrating into mC2 
int GetCrossfadeTime() {return 0;}; // TuniacApp.Preferences
bool GetActivePlaylistCheckNext() { return false; }; //TuniacApp.PLaylistManager
};

}}} // NS
