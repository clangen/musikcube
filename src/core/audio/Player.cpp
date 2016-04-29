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

#include "pch.hpp"

#include <core/audio/Player.h>
#include <core/PluginFactory.h>

using namespace musik::core::audio;

PlayerPtr Player::Create(utfstring &url, OutputPtr *output) {
    return PlayerPtr(new Player(url,output));
}

Player::Player(utfstring &url, OutputPtr *output)
 :volume(1.0)
 ,state(Player::Precache)
 ,url(url)
 ,totalBufferSize(0)
 ,maxBufferSize(2000000)
 ,currentPosition(0)
 ,setPosition(-1)
{
    if (*output) {
        this->output = *output;
    }
    else{
        /* if no output is specified, find all output plugins, and select the first one. */
        typedef std::vector<OutputPtr> OutputVector;

        OutputVector outputs = musik::core::PluginFactory::Instance().QueryInterface<
            IOutput, musik::core::PluginFactory::DestroyDeleter<IOutput>>("GetAudioOutput");

        if (!outputs.empty()) {
            this->output = outputs.front();
        }
    }

    /* each player instance is driven by a background thread. start it. */
    this->thread.reset(new boost::thread(boost::bind(&Player::ThreadLoop,this)));
}

Player::~Player() {
    this->Stop();

    if (this->thread) {
        this->thread->join();
    }
}

void Player::Play() {
    boost::mutex::scoped_lock lock(this->mutex);
    this->state = Player::Playing;
    this->waitCondition.notify_all();
}

void Player::Stop() {
    {
        boost::mutex::scoped_lock lock(this->mutex);
        this->state = Player::Quit;
        this->bufferQueue.clear();
    }
    
    /* TODO: why are we caching this locally? */
    OutputPtr output = this->output;
    if(output) {
        output->ClearBuffers();
    }
}

void Player::Pause() {
    if (this->output) {
        this->output->Pause();
    }
}

void Player::Resume() {
    if (this->output) {
        this->output->Resume();
    }
}

double Player::Position() {
    boost::mutex::scoped_lock lock(this->mutex);
    return this->currentPosition;
}

void Player::SetPosition(double seconds) {
    boost::mutex::scoped_lock lock(this->mutex);
    this->setPosition = seconds;
}

double Player::Volume() {
    boost::mutex::scoped_lock lock(this->mutex);
    return this->volume;
}

void Player::SetVolume(double volume) {
    boost::mutex::scoped_lock lock(this->mutex);

    this->volume = volume;

    if (this->output) {
        this->output->SetVolume(this->volume);
    }
}

int Player::State() {
    boost::mutex::scoped_lock lock(this->mutex);
    return this->state;
}

void Player::ThreadLoop() {
    /* create and open the stream */
    this->stream = Stream::Create();
    if (this->stream->OpenStream(this->url)) {
        /* ensure the volume is set properly */
        {
            boost::mutex::scoped_lock lock(this->mutex);
            this->output->SetVolume(this->volume);
        }

        /* precache until buffers are full */
        bool keepPrecaching = true;
        while(this->State() == Precache && keepPrecaching) {
            keepPrecaching = this->PreBuffer();
            boost::thread::yield();
        }

        /* wait until we enter the Playing or Quit state; we may still
        be in the Precache state, but with full buffers */
        {
            boost::mutex::scoped_lock lock(this->mutex);
            while (this->state == Precache) {
                this->waitCondition.wait(lock);
            }
        }

        this->PlaybackStarted(this);

        /* we're ready to go.... */
        bool finished = false;
        while(!finished && !this->Exited()) {
            /* see if we've been asked to seek since the last sample was
            played. if we have, clear our output buffer and seek the 
            stream. */
            if (this->setPosition != -1) {
                this->output->ClearBuffers();
                this->stream->SetPosition(this->setPosition);

                {
                    boost::mutex::scoped_lock lock(this->mutex);
                    this->bufferQueue.clear();
                    this->setPosition = -1;
                    this->totalBufferSize = 0;
                }
            }

            /* TODO: why do we release buffers from the output here?? */
            this->output->ReleaseBuffers();

            BufferPtr buffer;

            /* the buffer queue may already have some buffers available if it was
            prefetched. */
            if (!this->BufferQueueEmpty()) {
                boost::mutex::scoped_lock lock(this->mutex);
                buffer = this->bufferQueue.front();
            }
            /* otherwise, we need to grab a buffer from the stream and add it to the queue */
			else {
                buffer  = this->stream->NextBuffer();
                if (buffer) {
                    boost::mutex::scoped_lock lock(this->mutex);
                    this->bufferQueue.push_back(buffer);
                    this->totalBufferSize += buffer->Bytes();
                }
            }

            /* if we have a buffer available, let's try to send it to the output device */
            if (buffer) {
                {
                    // Add the buffer to locked buffers so the output do not have time to play and 
                    // try to release the buffer before we have to add it.
                    boost::mutex::scoped_lock lock(this->mutex);
                    this->lockedBuffers.push_back(buffer);
                }

                // Try to play the buffer
                if (!this->output->PlayBuffer(buffer.get(), this)) {
                    {
                        // We didn't manage to play the buffer, remove it from the locked buffer queue
                        boost::mutex::scoped_lock lock(this->mutex);
                        this->lockedBuffers.pop_back();
                    }

                    if(!this->PreBuffer()) {
                        // Wait for buffersize to become smaller
                        boost::mutex::scoped_lock lock(this->mutex);
                        if(this->totalBufferSize>this->maxBufferSize) {
                            this->waitCondition.wait(lock);
                        }
                    }
                }
				else{
                    // Buffer send to output
                    boost::mutex::scoped_lock lock(this->mutex);
                    if(!this->bufferQueue.empty()) {
                        this->bufferQueue.pop_front();

                        // Set currentPosition
                        if(this->lockedBuffers.size() == 1){
                            this->currentPosition = buffer->Position();
                        }
                    }
                }
            }

            /* if we're unable to obtain a buffer, it means we're out of data and the
            player is finished. terminate the thread. */
			else {
                finished = true;
            }
        }

        /* if the Quit flag isn't set, that means the stream has sent the final
        buffers to the output, and is about to end. raise the "almost ended"
        event. */
        if (!this->Exited()) {
            this->PlaybackAlmostEnded(this);
        }
    }

    /* if the stream failed to open... */
	else {
        this->PlaybackError(this);
    }

    /* need to wait until all the buffers have been released */
    this->ReleaseAllBuffers();

    /* set the final state */
    {
        boost::mutex::scoped_lock lock(this->mutex);
        this->state = Player::Quit;
    }

    this->PlaybackEnded(this);

    this->output.reset();
    this->stream.reset();
}

void Player::ReleaseAllBuffers() {
    this->output->ReleaseBuffers();
        
    {
        boost::mutex::scoped_lock lock(this->mutex);
        this->lockedBuffers.empty();

        while (this->state != Player::Quit) {
            this->waitCondition.wait(lock);
        }
    }
}

bool Player::BufferQueueEmpty() {
    boost::mutex::scoped_lock lock(this->mutex);
    return this->bufferQueue.empty();
}

bool Player::PreBuffer() {
    /* don't prebuffer if the buffer is already full */
    if(this->totalBufferSize > this->maxBufferSize) {
        return false;
    }
	else {
        BufferPtr newBuffer = this->stream->NextBuffer();

        if (newBuffer) {
            boost::mutex::scoped_lock lock(this->mutex);
            this->bufferQueue.push_back(newBuffer);
            this->totalBufferSize += newBuffer->Bytes();
            this->waitCondition.notify_all(); /* TODO: what's waiting on this? */
        }

        return true;
    }
}

bool Player::Exited() {
    boost::mutex::scoped_lock lock(this->mutex);
    return (this->state == Player::Quit);
}

void Player::OnBufferProcessed(IBuffer *buffer) {
    boost::mutex::scoped_lock lock(this->mutex);

    /* removes the specified buffer from the list of locked buffers, and also
    removes it from the stream. after this, recalculate the current position */
    BufferList::iterator it = this->lockedBuffers.begin();
    for ( ; it != this->lockedBuffers.end(); ++it) {
        if (it->get() == buffer) {
            this->totalBufferSize -= buffer->Bytes();

			if (this->stream) {
                this->stream->DeleteBuffer(*it);
            }

            this->lockedBuffers.erase(it);

            if (!this->lockedBuffers.empty()) {
                this->currentPosition = this->lockedBuffers.front()->Position();
            }

            this->waitCondition.notify_all(); /* TODO: what's waiting on this? */

            return;
        }
    }
}

void Player::Notify() {
    this->waitCondition.notify_all();
}



