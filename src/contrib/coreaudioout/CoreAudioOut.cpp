#include "CoreAudioOut.h"

#include <iostream>

#define BUFFER_COUNT 8

using namespace musik::core::audio;

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
    boost::recursive_mutex::scoped_lock lock(this->mutex);

    auto it = this->buffers.begin();
    while (it != this->buffers.end()) {
        if (*it == context) {
            this->buffers.erase(it);
        }
        ++it;
    }

    context->provider->OnBufferProcessed(context->buffer);
    delete context;
}

CoreAudioOut::CoreAudioOut() {
    this->quit = false;
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

    this->audioQueue = NULL;
}

bool CoreAudioOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    boost::recursive_mutex::scoped_lock lock(this->mutex);

    if (countBuffersWithProvider(this->buffers, provider) >= BUFFER_COUNT) {
        /* enough buffers are already in the queue. bail, we'll notify the
        caller when there's more data available */
        return false;
    }

    OSStatus result;

    if (buffer->SampleRate() != this->audioFormat.mSampleRate ||
        buffer->Channels() != this->audioFormat.mChannelsPerFrame ||
        this->audioQueue == NULL)
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
            NULL,
            kCFRunLoopCommonModes,
            0,
            &this->audioQueue);

        if (result != 0) {
            std::cerr << "AudioQueueNewOutput failed: " << result << "\n";
            return false;
        }

        result = AudioQueueStart(this->audioQueue, NULL);

        this->SetVolume(volume);

        if (result != 0) {
            std::cerr << "AudioQueueStart failed: " << result << "\n";
            return false;
        }
    }

    AudioQueueBufferRef audioQueueBuffer = NULL;

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
        this->audioQueue, audioQueueBuffer, 0, NULL);

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

void CoreAudioOut::Pause() {
    boost::recursive_mutex::scoped_lock lock(this->mutex);

    if (this->audioQueue) {
        AudioQueuePause(this->audioQueue);
    }
}

void CoreAudioOut::Resume() {
    boost::recursive_mutex::scoped_lock lock(this->mutex);

    if (this->audioQueue) {
        AudioQueueStart(this->audioQueue, NULL);
    }
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

void CoreAudioOut::Stop() {
    AudioQueueRef queue = NULL;

    {
        /* AudioQueueStop/AudioQueueDispose will trigger the callback, which
        will try to dispose of the samples on a separate thread and deadlock.
        cache the queue and reset outside of the critical section */
        boost::recursive_mutex::scoped_lock lock(this->mutex);
        queue = this->audioQueue;
        this->audioQueue = NULL;
    }

    if (queue) {
        AudioQueueStop(queue, true);
        AudioQueueDispose(queue, true);
    }
}
