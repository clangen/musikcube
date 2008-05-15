#include "pch.hpp"

#include <core/audio/AudioStream.h>
#include <core/audio/IAudioOutput.h>
#include <core/audio/IAudioSource.h>
#include <core/audio/Transport.h>

using namespace musik::core::audio;

unsigned long AudioStream::streamsCreated = 0;

AudioStream::AudioStream(IAudioSource* source, IAudioOutput* output, Transport* owner) 
: audioSource(source)
, transport(owner)
, playState(PlayStateUnknown)
, fadeState(FadeStateNone)
, volume(1)
, mixNotify(false)
, isFinished(false)
, isLast(false)
, samplesOut(0)
{
    this->output = output;
	this->output->SetCallback(this);

	unsigned long srate;
	source->GetFormat(&srate, &this->channels);
	this->output->SetFormat(srate, this->channels);

	this->packetizer.SetPacketSize(this->output->GetBlockSize());

    this->streamId = ++AudioStream::streamsCreated;
}

AudioStream::~AudioStream()
{
    this->output->Destroy();
	this->audioSource->Destroy();
}

bool			AudioStream::SetVolumeScale(float scale)
{
	this->volumeScale = scale;
	return true;
}

bool			AudioStream::GetBuffer(float * pAudioBuffer, unsigned long NumSamples)
{
    boost::mutex::scoped_lock lock(this->mutex);

    while (this->playState == PlayStatePaused)
    {
        this->pauseCondition.wait(lock);
    }

    if (this->isFinished)
    {
        return false;
    }

	if(this->packetizer.IsFinished())
	{
		if(!this->mixNotify)
		{
            transport->EventMixpointReached();
			this->mixNotify = true;
		}
		this->isFinished = true;
	}

	while(!this->packetizer.IsBufferAvailable())
	{
		float *			pBuffer			= NULL;
		unsigned long	ulNumSamples	= 0;

		if(this->audioSource->GetBuffer(&pBuffer, &ulNumSamples))
		{
			this->packetizer.WriteData(pBuffer, ulNumSamples);
		}
		else
		{
			this->packetizer.Finished();
		}
	}

	if(this->packetizer.GetBuffer(pAudioBuffer))
	{
		// if we're crossfading
        if(this->fadeState != FadeStateNone)
		{
			for(unsigned long x=0; x<NumSamples; x+=this->channels)
			{
				for(unsigned long chan=0; chan<this->channels; chan++)
				{
					pAudioBuffer[x+chan]		*= this->volume;
				}
				this->volume += this->volumeChange;
				this->volume = max(0.0f, min(this->volume, 1.0f));
			}

			if((this->volume == 0.0) || (this->volume == 1.0))
                this->fadeState = FadeStateNone;
		}

		// now apply the volume scale, only if we need to
		if(this->volumeScale != 1.0)
		{
			for(unsigned long x=0; x<NumSamples; x+=this->channels)
			{
				for(unsigned long chan=0; chan<this->channels; chan++)
				{
					pAudioBuffer[x+chan]		*= this->volumeScale;
				}
			}
		}

		this->packetizer.Advance();
		this->samplesOut +=  NumSamples;

		unsigned long pos = this->PositionMs();
		unsigned long len = this->LengthMs();
		unsigned long cft = this->GetCrossfadeTime() * 1000;

		if(!this->mixNotify)
		{
            if (len <= cft || pos >= (len - cft))
			{
				this->isLast = !GetActivePlaylistCheckNext();
				transport->EventMixpointReached();
				this->mixNotify = true;
			}
		}
		else
		{
			//if at end of last song in playlist but crossfade was the call n seconds ago
			//used for repeatnone where this is the end of line. 
			if(pos >= len && this->isLast)
			{
				transport->EventPlaybackStoppedOk();

                this->playState = PlayStateStopped;
			}
		}

		return true;
	}

	return false;
}

bool AudioStream::Start()
{
	if (this->output->Start())
    {
        this->playState = PlayStatePlaying;
        return true;
    }
    else
    {
        return false;
    }
}

bool AudioStream::Stop()
{
    if ((this->output != 0) || this->output->Stop())
    {
        this->playState = PlayStateStopped;

        this->pauseCondition.notify_one();

        return true;
    }
    else
    {
        return false;
    }
}

bool AudioStream::Pause()
{
    boost::mutex::scoped_lock   lock(this->mutex);

    this->playState = PlayStatePaused;

    return true;
}

bool AudioStream::Resume()
{
    boost::mutex::scoped_lock   lock(this->mutex);

    this->playState = PlayStatePlaying;

    this->pauseCondition.notify_one();

    return true;
}

unsigned long	AudioStream::LengthMs() const
{
	unsigned long length;

	if(this->audioSource->GetLength(&length))
		return length;

    return AudioStream::UnknownLength;
}

unsigned long	AudioStream::PositionMs() const
{
	unsigned long msOutput = ((float)this->samplesOut / (((float)this->output->GetSampleRate()/1000.0f) * (float)this->output->GetChannels()));
	return msOutput;
}

bool    AudioStream::SetPositionMs(unsigned long ms)
{
    boost::mutex::scoped_lock lock(this->mutex);

    if(this->fadeState != FadeStateNone)
	{
		this->volume		= 1.0;
        this->fadeState = FadeStateNone;
	}

	unsigned long Pos = ms;

	if(this->audioSource->SetPosition(&Pos))
	{
		this->samplesOut = Pos * (((float)this->output->GetSampleRate()/1000.0f) * (float)this->output->GetChannels());

		this->packetizer.Reset();
		this->output->Reset();

		return true;
	}

	return false;
}

utfstring AudioStream::ToString() const
{
    std::utfstringstream ss;

    ss << this->streamId << " " << this->audioSource->GetSource();

    return ss.str();
}