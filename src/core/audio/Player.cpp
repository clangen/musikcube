//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, Daniel Önnerby
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
#ifdef WIN32
#include "pch.hpp"
#else
#include <core/pch.hpp>
#endif

#include <core/audio/Player.h>
#include <core/PluginFactory.h>

using namespace musik::core::audio;

PlayerPtr Player::Create(utfstring &url,OutputPtr *output){
    return PlayerPtr(new Player(url,output));
}

Player::Player(utfstring &url,OutputPtr *output)
 :volume(1.0)
 ,state(Player::Precache)
 ,url(url)
 ,totalBufferSize(0)
 ,maxBufferSize(2000000)
 ,currentPosition(0)
 ,setPosition(-1)
{
    if(*output){
        this->output    = *output;
    }else{
        // Start by finding out what output to use
        typedef std::vector<OutputPtr> OutputVector;
        OutputVector outputs = musik::core::PluginFactory::Instance().QueryInterface<
            IOutput,
            musik::core::PluginFactory::DestroyDeleter<IOutput> >("GetAudioOutput");

        if(!outputs.empty()){
            // Get the firstt available output
            this->output    = outputs.front();
//            this->output->Initialize(this);
        }
    }

    // Start the thread
    this->thread.reset(new boost::thread(boost::bind(&Player::ThreadLoop,this)));

}

Player::~Player(void){
    this->Stop();
    if(this->thread){
        this->thread->join();
    }
}

void Player::Play(){
    boost::mutex::scoped_lock lock(this->mutex);
    this->state    = Player::Playing;
    this->waitCondition.notify_all();
}

void Player::Stop(){
    {
        boost::mutex::scoped_lock lock(this->mutex);
        this->state    = Player::Quit;
        this->bufferQueue.clear();
    }
    
    OutputPtr theOutput(this->output);
    if(theOutput){
        this->output->ClearBuffers();
    }

}

void Player::Pause(){
    if(this->output){
        this->output->Pause();
    }
}

void Player::Resume(){
    if(this->output){
        this->output->Resume();
    }
}

double Player::Position(){
    boost::mutex::scoped_lock lock(this->mutex);
    return this->currentPosition;
}

void Player::SetPosition(double seconds){
    boost::mutex::scoped_lock lock(this->mutex);
    this->setPosition   = seconds;
}

double Player::Volume(){
    boost::mutex::scoped_lock lock(this->mutex);
    return this->volume;
}

void Player::SetVolume(double volume){
    boost::mutex::scoped_lock lock(this->mutex);
    this->volume    = volume;
    if(this->output){
        this->output->SetVolume(this->volume);
    }
}

int Player::State(){
    boost::mutex::scoped_lock lock(this->mutex);
    return this->state;
}

void Player::ThreadLoop(){
#ifdef _DEBUG
	std::cerr << "Player::ThreadLoop started" << std::endl;
#endif
    // First start the stream
    this->stream    = Stream::Create();
    if(this->stream->OpenStream(this->url)){
        {
            boost::mutex::scoped_lock lock(this->mutex);
            // Set the volume in the output
            this->output->SetVolume(this->volume);
        }

        // If it's not started, lets precache
        bool keepPrecaching(true);
        while(this->State()==Precache && keepPrecaching){
            keepPrecaching  = this->PreBuffer();
            boost::thread::yield();
        }

        // Lets wait until we are not precaching anymore
        {
            boost::mutex::scoped_lock lock(this->mutex);
            while(this->state==Precache){
                this->waitCondition.wait(lock);
            }
        }

        this->PlaybackStarted(this);

        // Player should be started or quit by now
        bool finished(false);
        while(!finished && !this->Exited()){

            if(this->setPosition!=-1){
                // Set a new position
                this->output->ClearBuffers();
                this->stream->SetPosition(this->setPosition);
                {
                    boost::mutex::scoped_lock lock(this->mutex);
                    this->bufferQueue.clear();
//                    this->lockedBuffers.clear();
                    this->setPosition       = -1;
                    this->totalBufferSize   = 0;
                }
            }

            this->output->ReleaseBuffers();

            // Get a buffer, either from the bufferQueue, or from the stream
            BufferPtr buffer;

            if(!this->BufferQueueEmpty()){
                boost::mutex::scoped_lock lock(this->mutex);
                buffer  = this->bufferQueue.front();
            }else{
                buffer  = this->stream->NextBuffer();
                if(buffer){
                    boost::mutex::scoped_lock lock(this->mutex);
                    this->bufferQueue.push_back(buffer);
                    this->totalBufferSize += buffer->Bytes();
                }
            }

            if(buffer){

                {
                    // Add the buffer to locked buffers so the output do not have time to play and 
                    // try to release the buffer before we have to add it.
                    boost::mutex::scoped_lock lock(this->mutex);
                    this->lockedBuffers.push_back(buffer);
                }

                // Try to play the buffer
                if(!this->output->PlayBuffer(buffer.get(),this)){
                    {
                        // We didn't manage to play the buffer, remove it from the locked buffer queue
                        boost::mutex::scoped_lock lock(this->mutex);
                        this->lockedBuffers.pop_back();
                    }

                    if(!this->PreBuffer()){

                        // Wait for buffersize to become smaller
                        boost::mutex::scoped_lock lock(this->mutex);
                        if(this->totalBufferSize>this->maxBufferSize){
                            this->waitCondition.wait(lock);
                        }

                    }
                }else{
                    // Buffer send to output
                    boost::mutex::scoped_lock lock(this->mutex);
                    if(!this->bufferQueue.empty()){
                        this->bufferQueue.pop_front();

                        // Set currentPosition
                        if(this->lockedBuffers.size()==1){
                            this->currentPosition   = buffer->Position();
                        }
                    }

                }
            }else{
                // We have no more to decode
                finished    = true;
            }
        }

        // TODO: call a signal to notify that player is almost done
        if(!this->Exited()){
            this->PlaybackAlmostEnded(this);
        }

        // We need to wait for all the lockedBuffers to be released
        bool buffersEmpty=false;
        do{
            this->output->ReleaseBuffers();
            {
                boost::mutex::scoped_lock lock(this->mutex);
                buffersEmpty    = this->lockedBuffers.empty();
                if(!buffersEmpty && this->state!=Player::Quit){
                    this->waitCondition.wait(lock);
                }
            }
        }while(!buffersEmpty && !this->Exited());

    }else{
        // Unable to open stream
        this->PlaybackError(this);
    }

    {
        boost::mutex::scoped_lock lock(this->mutex);
        this->state    = Player::Quit;
    }

    this->PlaybackEnded(this);

    this->output->ReleaseBuffers();

    this->output.reset();
    this->stream.reset();

}

bool Player::BufferQueueEmpty(){
    boost::mutex::scoped_lock lock(this->mutex);
    return this->bufferQueue.empty();
}

bool Player::PreBuffer(){
    // But not if buffer is full
    if(this->totalBufferSize>this->maxBufferSize){
        return false;
    }else{
        BufferPtr newBuffer = this->stream->NextBuffer();
        if(newBuffer){
            boost::mutex::scoped_lock lock(this->mutex);
            this->bufferQueue.push_back(newBuffer);
            this->totalBufferSize += newBuffer->Bytes();
            this->waitCondition.notify_all();
        }
        return true;
    }
}

bool Player::Exited(){
    boost::mutex::scoped_lock lock(this->mutex);
    return this->state==Player::Quit;
}

void Player::ReleaseBuffer(IBuffer *buffer){
    boost::mutex::scoped_lock lock(this->mutex);
    // Remove the buffer from lockedBuffers
    for(BufferList::iterator foundBuffer=this->lockedBuffers.begin();foundBuffer!=this->lockedBuffers.end();++foundBuffer){
        if(foundBuffer->get()==buffer){
            this->totalBufferSize -= buffer->Bytes();
            if( this->stream ){
                this->stream->DeleteBuffer(*foundBuffer);
            }
            this->lockedBuffers.erase(foundBuffer);

            // Calculate current position from front locked buffer
            if(!this->lockedBuffers.empty()){
                this->currentPosition   = this->lockedBuffers.front()->Position();
            }

            // Notify
            this->waitCondition.notify_all();
            return;
        }
    }
    // We should never reach this point
    //throw "Releasing nonexisting buffer";
}

void Player::Notify(){
    this->waitCondition.notify_all();
}



