//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, Björn Olievier
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright 
//      notice, this list of conditions and the following disclaimer in the 
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may 
//      be used to endorse or promote products derived from this software 
//      without specific prior written permission. 
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE. 
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.hpp"

#include <core/audio/Transport.h>

#include <core/PluginFactory.h>

#include <core/audio/AudioStream.h>
#include <core/audio/IAudioOutput.h>
#include <core/audio/IAudioSource.h>


using namespace musik::core;
using namespace musik::core::audio;

Transport::Transport() 
: currVolume(100) //TODO: preference or previous value
, activeStream(NULL)
{
    this->registeredSourceSuppliers = PluginFactory::Instance().QueryInterface<
        IAudioSourceSupplier,
        PluginFactory::DestroyDeleter<IAudioSourceSupplier> >("CreateAudioSourceSupplier");

    this->registeredOutputSuppliers = PluginFactory::Instance().QueryInterface<
        IAudioOutputSupplier,
        PluginFactory::DestroyDeleter<IAudioOutputSupplier> >("CreateAudioOutputSupplier");
}

Transport::~Transport()
{
    for (size_t i=0; i<this->openStreams.size(); ++i)
    {
        this->openStreams[i]->Stop();
        delete this->openStreams[i];
    }
    this->openStreams.clear();

    this->activeStream = NULL;
}

void Transport::Start(TrackPtr trackPtr)
{
    this->RemoveFinishedStreams();

    bool success = false;

    AudioStream * audioStream = this->CreateStream(trackPtr);

    if (audioStream != NULL)
    {
        audioStream->SetVolumeScale(this->currVolume/100.0f);

        if (audioStream->Start())
        {
            this->openStreams.push_back(audioStream);

            success = true;
        }
    }

    if (success)
    {
        this->activeStream = audioStream;
        this->EventPlaybackStartedOk(trackPtr);
    }
    else
    {
        this->EventPlaybackStartedFail(trackPtr);
    }
}

void Transport::Stop()
{
    //TODO: review

    this->RemoveFinishedStreams();

    if (this->openStreams.empty())
    {
//        this->EventPlaybackStoppedFail(); //TODO: still necessary ?
        return;
    }

    AudioStream* stream = this->openStreams[0];
    
    if (stream->Stop())
    {
        if (stream == this->activeStream)
            this->activeStream = NULL;

        this->EventPlaybackStoppedOk(stream->Track());        
        
        delete stream;

        this->openStreams.erase(this->openStreams.begin());
    }
    else
    {
        this->EventPlaybackStoppedFail(stream->Track());
    }
}

bool Transport::Pause()
{
    if (this->openStreams.empty())
    {
        this->EventPlaybackPausedFail();
        return false;
    }

    std::vector<AudioStream*>::iterator it;
    bool ret = true;

    for(it = this->openStreams.begin(); it != this->openStreams.end(); it++)
    {
        AudioStream* stream = *(it);
        ret &= stream->Pause();
    }
    
    if (ret)    this->EventPlaybackPausedOk();
    else        this->EventPlaybackPausedFail();

    return ret;
}

bool Transport::Resume()
{
    if (this->openStreams.empty())
    {
        this->EventPlaybackResumedFail();
        return false;
    }

    std::vector<AudioStream*>::iterator it;
    bool ret = true;

    for(it = this->openStreams.begin(); it != this->openStreams.end(); it++)
    {
        AudioStream* stream = *(it);
        ret &= stream->Resume();
    }

    if (ret)    this->EventPlaybackResumedOk();
    else        this->EventPlaybackResumedFail();
    return ret;
}

void Transport::SetTrackPosition(unsigned long position)
{
    AudioStream* stream = this->openStreams[0];

    stream->SetPositionMs(position);
}

unsigned long Transport::TrackPosition() const
{
    if (this->activeStream)     return this->activeStream->PositionMs(); 
    else                        return 0;
}

unsigned long Transport::TrackLength() const
{
    if (this->activeStream)   return this->activeStream->LengthMs();
    else                      return 0;
}

void Transport::SetVolume(short volume)
{
    if (volume < 0 || volume > 100)
    {
        this->EventVolumeChangedFail();
        return;
    }

    std::vector<AudioStream*>::iterator it;

    for(it = this->openStreams.begin(); it != this->openStreams.end(); it++)
    {
        AudioStream* stream = *(it);
        stream->SetVolumeScale(volume/100.0f);
    }    

    this->currVolume = volume;

    this->EventVolumeChangedOk();
}

size_t Transport::NumOfStreams() const
{
    return this->openStreams.size();
}

AudioStreamOverview Transport::StreamsOverview() const
{
	AudioStreamOverview	overview;

	for(std::vector<AudioStream*>::const_iterator it = this->openStreams.begin()
	   ;it != this->openStreams.end()
	   ;it++
	   )
    {
		overview.push_back((*it)->ToString());
    }    

	return overview;
}

AudioStream* Transport::CreateStream(TrackPtr  trackPtr)
{
    boost::shared_ptr<IAudioSourceSupplier> supplier;
    IAudioSource* audioSource;
    AudioStream* audioStream = NULL;

    if (this->registeredOutputSuppliers.size() == 0)
    {
        return 0;
    }

    IAudioOutput* audioOutput =
        this->registeredOutputSuppliers[0]->CreateAudioOutput();  

    const utfchar* filePath = trackPtr->GetValue("path");

    SourceSupplierList::const_iterator it;
    for(it = this->registeredSourceSuppliers.begin(); it != this->registeredSourceSuppliers.end(); it++)
    {
        supplier = *it;

        if (supplier->CanHandle(filePath))
        {
            break;
        }
    }             
    
    if (it != this->registeredSourceSuppliers.end() && supplier)
    {
        audioSource = supplier->CreateAudioSource();
	
        if (audioSource != NULL && audioSource->Open(filePath))
	    {
		    audioStream = new AudioStream(audioSource, audioOutput, this, trackPtr);
        }
    }

    return audioStream;
}

void Transport::RemoveFinishedStreams()
{
    for (size_t i=0; i<this->openStreams.size(); ++i)
    {
        AudioStream* stream = this->openStreams[i];
        if (stream->isFinished)
        {
            stream->Stop();
            delete stream;
            this->openStreams.erase(this->openStreams.begin() + i);
            --i;
        }
    }
}