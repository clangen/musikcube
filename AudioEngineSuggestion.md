## _Draft_ ##

# AudioEngine suggestion #

## AudioStream: ##
The AudioStream should be a class for streaming decoded audio. The class should be threaded

and handle decoding and buffering of data. It should also handle the IDSP plugins in it's

own internal thread.
External interface:
```
Open(const utfchar *path)
Close()
Channels()
SampleRate()
Seek(long miliseconds) // This should flush the internal buffer
GetBuffer(const void* buffer,nofSamples) // total buffer size should be 

nofSamples*sizeof(float)*Channels()
long Length(); // returns miliseconds
```

## AudioPlayer: ##
A AudioPlayer is a class to play one stream. It basically connects the AudioStream and a

AudioOutput to be able to play a track.
External interface:
```
Play(const utfchar *path);
Stop();
Pause();
Resume();
long Length(); // returns miliseconds
long CurrentPosition(); // in miliseconds
SetVolume(double volume);
double Volume();
// There should also be some events like
StartCrossMix;	// Event that is called when track passes the crossfade position
PlaybackStarted;
PlaybackEnded;
playbackError;
```

## Transporter: ##
The main playback interface.
Can contain multiple AudioPlayers (when crossmixing), but only one current.
External interface:
```
Play(const utfchar *path);
Stop();
Pause();
Resume();
SetVolume(double volume);
double Volume();
```

## IAudioSource (decoders) ##
The interface for creating new decoders.

## IAudioOutput (device playback) ##
External interface:
```
Play();
Pause();
Resume();
SetVolume(double volume);
double Volume();
```

## IDSP ##
DSP plugin interface

## ICrossMix ##
Crossmix (ie crossfade) plugin interface