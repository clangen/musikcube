//////////////////////////////////////////////////////////////////////////////
// Copyright � 2007, Daniel �nnerby
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

#include <core/debug.h>
#include <core/audio/Player.h>
#include <core/plugin/PluginFactory.h>
#include <algorithm>

#define MAX_PREBUFFER_QUEUE_COUNT 16

using namespace musik::core::audio;
using std::min;
using std::max;

static std::string TAG = "Player";

PlayerPtr Player::Create(const std::string &url, double volume, OutputPtr output) {
    return PlayerPtr(new Player(url, volume, output));
}

Player::OutputPtr Player::CreateDefaultOutput() {
    /* if no output is specified, find all output plugins, and select the first one. */
    typedef std::vector<OutputPtr> OutputVector;

    OutputVector outputs = musik::core::PluginFactory::Instance().QueryInterface<
        IOutput, musik::core::PluginFactory::DestroyDeleter<IOutput> >("GetAudioOutput");

    if (!outputs.empty()) {
        musik::debug::info(TAG, "found an IOutput device!");
        return outputs.front();
    }

    return Player::OutputPtr();
}


Player::Player(const std::string &url, double volume, OutputPtr output)
: volume(volume)
, state(Player::Precache)
, url(url)
, currentPosition(0)
, setPosition(-1) {
    musik::debug::info(TAG, "new instance created");

    /* we allow callers to specify an output device; but if they don't,
    we will create and manage our own. */
    this->output = output ? output : Player::CreateDefaultOutput();

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
    this->writeToOutputCondition.notify_all();
}

void Player::Stop() {
    {
        boost::mutex::scoped_lock lock(this->mutex);
        this->state = Player::Quit;
        this->prebufferQueue.clear();
        this->writeToOutputCondition.notify_all();
    }

    if (this->output) {
        this->output->Stop();
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
    this->setPosition = std::max(0.0, seconds);
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
        while (this->State() == Precache && keepPrecaching) {
            keepPrecaching = this->PreBuffer();
            boost::thread::yield();
        }

        /* wait until we enter the Playing or Quit state; we may still
        be in the Precache state. */
        {
            boost::mutex::scoped_lock lock(this->mutex);
            while (this->state == Precache) {
                this->writeToOutputCondition.wait(lock);
            }
        }

        this->PlaybackStarted(this);

        /* we're ready to go.... */
        bool finished = false;
        BufferPtr buffer;

        while (!finished && !this->Exited()) {
            /* see if we've been asked to seek since the last sample was
            played. if we have, clear our output buffer and seek the
            stream. */
            if (this->setPosition != -1) {
                this->output->Stop();

                while (this->lockedBuffers.size() > 0) {
                    writeToOutputCondition.wait(this->mutex);
                }

                this->stream->SetPosition(this->setPosition);

                {
                    boost::mutex::scoped_lock lock(this->mutex);
                    this->setPosition = -1;
                    this->prebufferQueue.clear();
                }

                buffer.reset();
            }

            /* let's see if we can find some samples to play */
            if (!buffer) {
                boost::mutex::scoped_lock lock(this->mutex);

                /* the buffer queue may already have some available if it was prefetched. */
                if (!this->prebufferQueue.empty()) {
                    buffer = this->prebufferQueue.front();
                    this->prebufferQueue.pop_front();
                }
                /* otherwise, we need to grab a buffer from the stream and add it to the queue */
                else {
                    buffer = this->stream->GetNextProcessedOutputBuffer();
                }
            }

            /* if we have a decoded, processed buffer available. let's try to send it to
            the output device. */
            if (buffer) {
                if (this->output->Play(buffer.get(), this)) {
                    /* success! the buffer was accepted by the output.*/
                    boost::mutex::scoped_lock lock(this->mutex);

                    /* lock it down so it's not destroyed until the output device lets us
                    know it's done with it. */
                    this->lockedBuffers.push_back(buffer);

                    if (this->lockedBuffers.size() == 1) {
                        this->currentPosition = buffer->Position();
                    }

                    buffer.reset(); /* important! we're done with this one locally. */
                }
                else {
                    /* the output device queue is full. we should block and wait until
                    the output lets us know that it needs more data */
                    boost::mutex::scoped_lock lock(this->mutex);
                    writeToOutputCondition.wait(this->mutex);
                }
            }
            /* if we're unable to obtain a buffer, it means we're out of data and the
            player is finished. terminate the thread. */
            else {
                finished = true;
            }
        }

        /* if the Quit flag isn't set, that means the stream has ended "naturally", i.e.
        it wasn't stopped by the user. raise the "almost ended" flag. */
        if (!this->Exited()) {
            this->PlaybackAlmostEnded(this);
        }
    }

    /* if the stream failed to open... */
    else {
        this->PlaybackError(this);
    }

    bool stopped = false;

    /* wait until all remaining buffers have been written, set final state... */
    {
        boost::mutex::scoped_lock lock(this->mutex);

        /* if the state is "Quit" that means the user terminated the stream, so no
        more buffers will be played / recycled from the output device. kill them now.*/
        if (this->state == Player::Quit) {
            this->lockedBuffers.clear();
        }
        /* otherwise wait for their completion */
        else {
            while (this->lockedBuffers.size() > 0) {
                writeToOutputCondition.wait(this->mutex);
            }
        }

        stopped = (this->state == Player::Quit);
        this->state = Player::Quit;
    }

    if (stopped) {
        this->PlaybackStopped(this);
    }
    else {
        this->PlaybackFinished(this);
    }

    this->output.reset();
    this->stream.reset();
}

void Player::ReleaseAllBuffers() {
    boost::mutex::scoped_lock lock(this->mutex);
    this->lockedBuffers.empty();
}

bool Player::PreBuffer() {
    /* don't prebuffer if the buffer is already full */
    if (this->prebufferQueue.size() < MAX_PREBUFFER_QUEUE_COUNT) {
        BufferPtr newBuffer = this->stream->GetNextProcessedOutputBuffer();

        if (newBuffer) {
            boost::mutex::scoped_lock lock(this->mutex);
            this->prebufferQueue.push_back(newBuffer);
        }

        return true;
    }

    return false;
}

bool Player::Exited() {
    boost::mutex::scoped_lock lock(this->mutex);
    return (this->state == Player::Quit);
}

void Player::OnBufferProcessed(IBuffer *buffer) {
    boost::mutex::scoped_lock lock(this->mutex);

    /* removes the specified buffer from the list of locked buffers, and also
    lets the stream know it can be recycled. */
    BufferList::iterator it = this->lockedBuffers.begin();
    for ( ; it != this->lockedBuffers.end(); ++it) {
        if (it->get() == buffer) {
            if (this->stream) {
                this->stream->OnBufferProcessedByPlayer(*it);
            }

            this->lockedBuffers.erase(it);

            if (!this->lockedBuffers.empty()) {
                this->currentPosition = this->lockedBuffers.front()->Position();
            }

            /* if the output device's internal buffers are full, it will stop
            accepting new samples. now that a buffer has been processed, we can
            try to enqueue another sample. the thread loop blocks on this condition */
            this->writeToOutputCondition.notify_all();

            return;
        }
    }
}
