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

#include "Transport.h"

#include <core/PluginFactory.h>

#include <core/audio/AudioStream.h>
#include <core/audio/IAudioOutput.h>
#include <core/audio/IAudioSource.h>

using namespace musik::core::audio;

Transport::Transport() :
    currVolume(100) //TODO: preference or previous value
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
}

void Transport::Start(const utfstring path)
{
    this->RemoveFinishedStreams();

    bool success = false;

    AudioStream * audioStream = this->CreateStream(path.c_str());

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
        this->PlaybackStartedOk();
    }
    else
    {
        this->PlaybackStartedFail();
    }
}

void Transport::Stop(size_t idx)
{
    this->RemoveFinishedStreams();

    if (this->openStreams.empty() || (idx < 0 || idx > this->openStreams.size()-1))
    {
        this->PlaybackStoppedFail();
        return;
    }

    AudioStream* stream = this->openStreams[idx];
    
    if (stream->Stop())
    {
        delete stream;

        this->openStreams.erase(this->openStreams.begin() + idx);

        this->PlaybackStoppedOk();
    }
    else
    {
        this->PlaybackStoppedFail();
    }
}

void Transport::JumpToPosition(short relativePosition)
{
    AudioStream* stream = this->openStreams[0];

    unsigned long posMS = stream->GetLength() * relativePosition / 100;
    
    stream->SetPosition(posMS);
}

void Transport::ChangeVolume(short volume)
{
    if (volume < 0 || volume > 100)
    {
        this->VolumeChangedFail();
        return;
    }

    std::vector<AudioStream*>::iterator it;

    for(it = this->openStreams.begin(); it != this->openStreams.end(); it++)
    {
        AudioStream* stream = *(it);
        stream->SetVolumeScale(volume/100.0f);
    }    

    this->currVolume = volume;

    this->VolumeChangedOk();
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

AudioStream* Transport::CreateStream(const utfstring sourcePath)
{
    boost::shared_ptr<IAudioSourceSupplier> supplier;
    IAudioSource* audioSource;
    AudioStream* audioStream = NULL;

    if (this->registeredOutputSuppliers.size() == 0)
    {
        return 0;
    }

    IAudioOutput* audioOutput =
        this->registeredOutputSuppliers[0]->CreateAudioOutput();  //TODO: deal with multiple outputs simultaneously

    SourceSupplierList::const_iterator it;
    for(it = this->registeredSourceSuppliers.begin(); it != this->registeredSourceSuppliers.end(); it++)
    {
        supplier = *it;

        if (supplier->CanHandle(sourcePath.c_str()))
        {
            break;
        }
    }             
    
    if (it != this->registeredSourceSuppliers.end() && supplier)
    {
        audioSource = supplier->CreateAudioSource();
	
        if (audioSource != NULL && audioSource->Open(sourcePath.c_str()))
	    {
		    audioStream = new AudioStream(audioSource, audioOutput, this);
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
