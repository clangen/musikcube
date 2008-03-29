#include "pch.hpp"

#include <core/audio/AudioStream.h>
#include <core/audio/IAudioSource.h>
#include <core/audio/IAudioOutput.h>
#include <core/audio/Transport.h>

using namespace musik::core::audio;

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

    if (this->isFinished)
    {
        return false;
    }

	if(this->packetizer.IsFinished())
	{
		if(!this->mixNotify)
		{
            transport->MixpointReached();
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

		unsigned long pos = this->GetPosition();
		unsigned long len = this->GetLength();
		unsigned long cft = this->GetCrossfadeTime() * 1000;

		if(!this->mixNotify)
		{
            if (len <= cft || pos >= (len - cft))
			{
				this->isLast = !GetActivePlaylistCheckNext();
				transport->MixpointReached();
				this->mixNotify = true;
			}
		}
		else
		{
			//if at end of last song in playlist but crossfade was the call n seconds ago
			//used for repeatnone where this is the end of line. 
			if(pos >= len && this->isLast)
			{
				transport->PlaybackStoppedOk();

                this->playState = PlayStateStopped;
			}
		}

		return true;
	}

	return false;
}
/*
unsigned long	AudioStream::GetState(void)
{
	return this->playState;
}

unsigned long	AudioStream::GetFadeState(void)
{
	return this->fadeState;
}

bool			AudioStream::IsFinished(void)
{
	return this->isFinished;
}

bool			AudioStream::FadeIn(unsigned long ulMS)
{
	AutoLock t(&this->lock);

    this->fadeState = FadeStateIn;
	this->volumeChange =  1.0f / ((float)ulMS * (float)this->output->GetSampleRate());
	this->volume = 0.0f;
	return true;
}

bool			AudioStream::FadeOut(unsigned long ulMS)
{
	AutoLock t(&this->lock);

    this->fadeState = FadeStateOut;
	this->volumeChange =  -(1.0f / ((float)ulMS * (float)this->output->GetSampleRate()));
	this->volume  = 1.0f;
	return true;
}
*/
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
        return true;
    }
    else
    {
        return false;
    }
}

unsigned long	AudioStream::GetLength() const
{
	unsigned long Length;

	if(this->audioSource->GetLength(&Length))
		return Length;

    return AudioStream::UnknownLength;
}

unsigned long	AudioStream::GetPosition() const
{
	unsigned long MSOutput = ((float)this->samplesOut / (((float)this->output->GetSampleRate()/1000.0f) * (float)this->output->GetChannels()));
	return MSOutput;
}

bool			AudioStream::SetPosition(unsigned long MS)
{
    boost::mutex::scoped_lock lock(this->mutex);

    if(this->fadeState != FadeStateNone)
	{
		this->volume		= 1.0;
        this->fadeState = FadeStateNone;
	}

	unsigned long Pos = MS;

	if(this->audioSource->SetPosition(&Pos))
	{
		this->samplesOut = Pos * (((float)this->output->GetSampleRate()/1000.0f) * (float)this->output->GetChannels());

		this->packetizer.Reset();
		this->output->Reset();

		return true;
	}

	return false;
}

/*
bool			AudioStream::GetVisData(float * ToHere, unsigned long ulNumSamples)
{
	return this->output->GetVisData(ToHere, ulNumSamples);
}
*/
