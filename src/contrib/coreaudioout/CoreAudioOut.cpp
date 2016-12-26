//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include "CoreAudioOut.h"

#include <iostream>

#define BUFFER_COUNT 32

using namespace musik::core::sdk;

void audioCallback(void *customData, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    CoreAudioOut* output = (CoreAudioOut *) customData;
    CoreAudioOut::BufferContext* context = (CoreAudioOut::BufferContext *) buffer->mUserData;

    OSStatus result = AudioQueueFreeBuffer(queue, buffer);

    if (result != 0) {
        std::cerr << "AudioQueueFreeBuffer failed: " << result << "\n";
    }

    output->NotifyBufferCompleted(context);
}

size_t countBuffersWithProvider(
    const std::list<CoreAudioOut::BufferContext*>& buffers,
    const IBufferProvider* provider)
{
    size_t count = 0;
    auto it = buffers.begin();
    while (it != buffers.end()) {
        if ((*it)->provider == provider) {
            ++count;
        }
        ++it;
    }
    return count;
}

void CoreAudioOut::NotifyBufferCompleted(BufferContext *context) {
    {
        boost::recursive_mutex::scoped_lock lock(this->mutex);

        bool found = false;
        auto it = this->buffers.begin();
        while (!found && it != this->buffers.end()) {
            if (*it == context) {
                this->buffers.erase(it);
                found = true;
            }
            else {
                ++it;
            }
        }
    }

    context->provider->OnBufferProcessed(context->buffer);
    delete context;
}

CoreAudioOut::CoreAudioOut() {
    this->quit = false;
    this->state = StateStopped;
    this->volume = 1.0f;

    this->audioFormat = (AudioStreamBasicDescription) { 0 };

    this->audioFormat.mFormatID = kAudioFormatLinearPCM;
    this->audioFormat.mFormatFlags = kAudioFormatFlagIsFloat;
    this->audioFormat.mFramesPerPacket = 1;
    this->audioFormat.mBitsPerChannel = 32;
    this->audioFormat.mReserved = 0;

    /* these get filled in later */
    this->audioFormat.mChannelsPerFrame = -1;
    this->audioFormat.mSampleRate = -1;
    this->audioFormat.mBytesPerFrame = -1;
    this->audioFormat.mBytesPerPacket = -1;

    this->audioQueue = nullptr;
}

bool CoreAudioOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    boost::recursive_mutex::scoped_lock lock(this->mutex);

    if (this->state != StatePlaying) {
        return false;
    }

    if (countBuffersWithProvider(this->buffers, provider) >= BUFFER_COUNT) {
        /* enough buffers are already in the queue. bail, we'll notify the
        caller when there's more data available */
        return false;
    }

    OSStatus result;

    if (buffer->SampleRate() != this->audioFormat.mSampleRate ||
        buffer->Channels() != this->audioFormat.mChannelsPerFrame ||
        this->audioQueue == nullptr)
    {
        this->audioFormat.mSampleRate = buffer->SampleRate();
        this->audioFormat.mChannelsPerFrame = buffer->Channels();
        this->audioFormat.mBytesPerFrame = (this->audioFormat.mBitsPerChannel / 8) * this->audioFormat.mChannelsPerFrame;
        this->audioFormat.mBytesPerPacket = this->audioFormat.mBytesPerFrame * this->audioFormat.mFramesPerPacket;

        this->Stop();

        result = AudioQueueNewOutput(
            &this->audioFormat,
            audioCallback,
            this,
            nullptr,
            kCFRunLoopCommonModes,
            0,
            &this->audioQueue);

        if (result != 0) {
            std::cerr << "AudioQueueNewOutput failed: " << result << "\n";
            return false;
        }

        result = AudioQueueStart(this->audioQueue, nullptr);

        this->SetVolume(volume);

        if (result != 0) {
            std::cerr << "AudioQueueStart failed: " << result << "\n";
            return false;
        }

        this->Resume();
    }

    AudioQueueBufferRef audioQueueBuffer = nullptr;

    size_t bytes = buffer->Bytes();

    result = AudioQueueAllocateBuffer(this->audioQueue, bytes, &audioQueueBuffer);

    BufferContext* context = new BufferContext();
    context->provider = provider;
    context->buffer = buffer;

    if (result != 0) {
        std::cerr << "AudioQueueAllocateBuffer failed: " << result << "\n";
        return false;
    }

    audioQueueBuffer->mUserData = (void *) context;
    audioQueueBuffer->mAudioDataByteSize = bytes;

    memcpy(
        audioQueueBuffer->mAudioData,
        (void *) buffer->BufferPointer(),
        bytes);

    result = AudioQueueEnqueueBuffer(
        this->audioQueue, audioQueueBuffer, 0, nullptr);

    if (result != 0) {
        std::cerr << "AudioQueueEnqueueBuffer failed: " << result << "\n";
        delete context;
        return false;
    }

    this->buffers.push_back(context);

    return true;
}

CoreAudioOut::~CoreAudioOut() {
    this->Stop();
}

void CoreAudioOut::Destroy() {
    delete this;
}

void CoreAudioOut::Drain() {
    boost::recursive_mutex::scoped_lock lock(this->mutex);

    if (this->state != StateStopped && this->audioQueue) {
        std::cerr << "CoreAudioOut: draining...\n";
        AudioQueueFlush(this->audioQueue);
        std::cerr << "CoreAudioOut: drained.\n";
    }
}

void CoreAudioOut::Pause() {
    boost::recursive_mutex::scoped_lock lock(this->mutex);

    if (this->audioQueue) {
        AudioQueuePause(this->audioQueue);
        this->state = StatePaused;
    }
}

void CoreAudioOut::Resume() {
    boost::recursive_mutex::scoped_lock lock(this->mutex);

    if (this->audioQueue) {
        AudioQueueStart(this->audioQueue, nullptr);
    }

    this->state = StatePlaying;
}

void CoreAudioOut::SetVolume(double volume) {
    boost::recursive_mutex::scoped_lock lock(this->mutex);

    this->volume = volume;

    if (this->audioQueue) {
        OSStatus result = AudioQueueSetParameter(
            this->audioQueue,
            kAudioQueueParam_Volume,
            volume);

        if (result != 0) {
            std::cerr << "AudioQueueSetParameter(volume) failed: " << result << "\n";
        }
    }
}

double CoreAudioOut::GetVolume() {
    return this->volume;
}

void CoreAudioOut::Stop() {
    AudioQueueRef queue = nullptr;

    {
        /* AudioQueueStop/AudioQueueDispose will trigger the callback, which
        will try to dispose of the samples on a separate thread and deadlock.
        cache the queue and reset outside of the critical section */
        boost::recursive_mutex::scoped_lock lock(this->mutex);
        queue = this->audioQueue;
        this->audioQueue = nullptr;
        this->state = StateStopped;
    }

    if (queue) {
        AudioQueueStop(queue, true);
        AudioQueueDispose(queue, true);
    }
}
