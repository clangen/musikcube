#include "CoreAudioOut.h"

#include <iostream>

#define BUFFER_COUNT 3

using namespace musik::core::audio;

void audioCallback(void *customData, AudioQueueRef queue, AudioQueueBufferRef buffer)
{
    OSStatus result = AudioQueueFreeBuffer(queue, buffer);

    if (result != 0) {
        std::cerr << "AudioQueueFreeBuffer failed: " << result << "\n";
    }

    CoreAudioOut* output = (CoreAudioOut *) customData;
    IBuffer* coreBuffer = (IBuffer *) buffer->mUserData;
    output->NotifyBufferCompleted(coreBuffer);
}

void CoreAudioOut::NotifyBufferCompleted(IBuffer *buffer) {
    boost::recursive_mutex::scoped_lock lock(this->mutex);
    --bufferCount;
    this->bufferProvider->OnBufferProcessed(buffer);
}

CoreAudioOut::CoreAudioOut() {
    this->bufferProvider = NULL;
    this->quit = false;
    this->bufferCount = 0;

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

    if (this->bufferCount >= BUFFER_COUNT) {
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

        if (result != 0) {
            std::cerr << "AudioQueueStart failed: " << result << "\n";
            return false;
        }
    }

    this->bufferProvider = provider;

    AudioQueueBufferRef audioQueueBuffer = NULL;

    size_t bytes = buffer->Bytes();

    result = AudioQueueAllocateBuffer(this->audioQueue, bytes, &audioQueueBuffer);

    if (result != 0) {
        std::cerr << "AudioQueueAllocateBuffer failed: " << result << "\n";
        return false;
    }

    audioQueueBuffer->mUserData = (void *) buffer;
    audioQueueBuffer->mAudioDataByteSize = bytes;

    memcpy(
        audioQueueBuffer->mAudioData,
        (void *) buffer->BufferPointer(),
        bytes);

    result = AudioQueueEnqueueBuffer(
        this->audioQueue, audioQueueBuffer, 0, NULL);

    if (result != 0) {
        std::cerr << "AudioQueueEnqueueBuffer failed: " << result << "\n";
        return false;
    }

    ++bufferCount;

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
