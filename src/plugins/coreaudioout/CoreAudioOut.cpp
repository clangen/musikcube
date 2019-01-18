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
#include <core/sdk/constants.h>
#include <core/sdk/IPreferences.h>
#include <iostream>
#include <vector>

#define BUFFER_COUNT 24
#define PREF_DEVICE_ID "device_id"

using namespace musik::core::sdk;

class CoreAudioDevice : public IDevice {
    public:
        CoreAudioDevice(const std::string& id, const std::string& name) {
            this->id = id;
            this->name = name;
        }

        virtual void Release() { delete this; }
        virtual const char* Name() const { return name.c_str(); }
        virtual const char* Id() const { return id.c_str(); }

    private:
        std::string name, id;
};

class CoreAudioDeviceList : public IDeviceList {
    public:
        virtual void Release() { delete this; }
        virtual size_t Count() const { return devices.size(); }
        virtual const IDevice* At(size_t index) const { return &devices.at(index); }

        void Add(const std::string& id, const std::string& name) {
            devices.push_back(CoreAudioDevice(id, name));
        }

    private:
        std::vector<CoreAudioDevice> devices;
};

static musik::core::sdk::IPreferences* prefs = nullptr;

extern "C" void SetPreferences(musik::core::sdk::IPreferences* prefs) {
    ::prefs = prefs;
    prefs->GetString(PREF_DEVICE_ID, nullptr, 0, "");
    prefs->Save();
}

static std::string getDeviceId() {
    return getPreferenceString<std::string>(prefs, PREF_DEVICE_ID, "");
}

void audioCallback(void *customData, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    CoreAudioOut* output = (CoreAudioOut *) customData;
    CoreAudioOut::BufferContext* context = (CoreAudioOut::BufferContext *) buffer->mUserData;

    OSStatus result = AudioQueueFreeBuffer(queue, buffer);

    if (result != 0) {
        std::cerr << "AudioQueueFreeBuffer failed: " << result << "\n";
    }

    output->NotifyBufferCompleted(context);
}

void CoreAudioOut::NotifyBufferCompleted(BufferContext *context) {
    {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);

        if (bufferCount > 0) {
            --bufferCount;
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
    this->bufferCount = 0;
}

int CoreAudioOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);

    if (this->state != StatePlaying) {
        return OutputInvalidState;
    }

    if (this->bufferCount >= BUFFER_COUNT) {
        /* enough buffers are already in the queue. bail, we'll notify the
        caller when there's more data available */
        return OutputBufferFull;
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

        lock.unlock();
        this->Stop();
        lock.lock();

        /* create the queue */
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
            return OutputInvalidState;
        }

        /* after the queue is created, but before it's started, let's make
        sure the correct output device is selected */
        auto device = this->GetDefaultDevice();
        if (device) {
            std::string deviceId = device->Id();
            if (deviceId.c_str()) {
                CFStringRef deviceUid = CFStringCreateWithBytes(
                    kCFAllocatorDefault,
                    (const UInt8*) deviceId.c_str(),
                    deviceId.size(),
                    kCFStringEncodingUTF8,
                    false);

                AudioQueueSetProperty(
                    this->audioQueue,
                    kAudioQueueProperty_CurrentDevice,
                    &deviceUid,
                    sizeof(deviceUid));

                CFRelease(deviceUid);
            }
            device->Release();
        }

        /* get it running! */
        result = AudioQueueStart(this->audioQueue, nullptr);

        this->SetVolume(volume);

        if (result != 0) {
            std::cerr << "AudioQueueStart failed: " << result << "\n";
            return OutputInvalidState;
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
        return OutputInvalidState;
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
        return OutputInvalidState;
    }

    ++bufferCount;

    return OutputBufferWritten;
}

CoreAudioOut::~CoreAudioOut() {
    this->Stop();
}

void CoreAudioOut::Release() {
    delete this;
}

void CoreAudioOut::Drain() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);

    if (this->state != StateStopped && this->audioQueue) {
        std::cerr << "CoreAudioOut: draining...\n";
        AudioQueueFlush(this->audioQueue);
        std::cerr << "CoreAudioOut: drained.\n";
    }
}

void CoreAudioOut::Pause() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);

    if (this->audioQueue) {
        AudioQueuePause(this->audioQueue);
        this->state = StatePaused;
    }
}

void CoreAudioOut::Resume() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);

    if (this->audioQueue) {
        AudioQueueStart(this->audioQueue, nullptr);
    }

    this->state = StatePlaying;
}

void CoreAudioOut::SetVolume(double volume) {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);

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
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        queue = this->audioQueue;
        this->audioQueue = nullptr;
        this->state = StateStopped;
        this->bufferCount = 0;
    }

    if (queue) {
        AudioQueueStop(queue, true);
        AudioQueueDispose(queue, true);
    }
}

IDeviceList* CoreAudioOut::GetDeviceList() {
    CoreAudioDeviceList* result = new CoreAudioDeviceList();

    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    UInt32 propsize;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &propsize) == 0) {
        int deviceCount = propsize / sizeof(AudioDeviceID);
        AudioDeviceID *deviceIds = new AudioDeviceID[deviceCount];

        char buffer[2048];
        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &propsize, deviceIds) == 0) {
            for (int i = 0; i < deviceCount; ++i) {
                auto deviceId = deviceIds[i];

                AudioObjectPropertyAddress outputAddress = {
                    kAudioDevicePropertyStreamConfiguration,
                    kAudioDevicePropertyScopeOutput,
                    0
                };

                /* see if this device has output channels. if it doesn't, it's a device
                that can only do input */
                size_t outputChannels = 0;
                if (AudioObjectGetPropertyDataSize(deviceId, &outputAddress, 0, NULL, &propsize) == 0) {
                    AudioBufferList *bufferList = (AudioBufferList *) malloc(propsize);
                    if (AudioObjectGetPropertyData(deviceId, &outputAddress, 0, NULL, &propsize, bufferList) == 0) {
                        for (UInt32 j = 0; j < bufferList->mNumberBuffers; ++j) {
                            outputChannels += bufferList->mBuffers[j].mNumberChannels;
                        }
                    }
                    free(bufferList);
                }

                if (outputChannels > 0) { /* device has an output! */
                    std::string deviceNameStr, deviceIdStr;

                    /* get the device name */

                    AudioObjectPropertyAddress nameAddress = {
                        kAudioDevicePropertyDeviceName,
                        kAudioDevicePropertyScopeOutput,
                        0
                    };

                    UInt32 maxLength = 2048;
                    if (AudioObjectGetPropertyData(deviceId, &nameAddress, 0, NULL, &maxLength, buffer) == 0) {
                        deviceNameStr = buffer;
                    }

                    /* get the device id */

                    AudioObjectPropertyAddress uidAddress = {
                        kAudioDevicePropertyDeviceUID,
                        kAudioObjectPropertyScopeGlobal,
                        kAudioObjectPropertyElementMaster
                    };

                    CFStringRef deviceUid;
                    if (AudioObjectGetPropertyData(deviceId, &uidAddress, 0, NULL, &propsize, &deviceUid) == 0) {
                        const char* cstr = CFStringGetCStringPtr(deviceUid, kCFStringEncodingUTF8);
                        if (cstr) {
                            deviceIdStr = cstr;
                        }
                    }

                    if (deviceNameStr.size() && deviceIdStr.size()) {
                        result->Add(deviceIdStr, deviceNameStr);
                    }
                }
            }
        }

        delete[] deviceIds;
    }

    return result;
}

bool CoreAudioOut::SetDefaultDevice(const char* deviceId) {
    return setDefaultDevice<IPreferences, CoreAudioDevice, IOutput>(prefs, this, PREF_DEVICE_ID, deviceId);
}

IDevice* CoreAudioOut::GetDefaultDevice() {
    return findDeviceById<CoreAudioDevice, IOutput>(this, getDeviceId());
}