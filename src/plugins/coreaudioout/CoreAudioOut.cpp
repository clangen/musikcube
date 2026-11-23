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
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {
    /* CF strings are not guaranteed to expose a direct UTF-8 pointer. copy
    through the conversion API and release every retained property value. */
    std::string stringProperty(AudioDeviceID device, AudioObjectPropertySelector selector) {
        const AudioObjectPropertyAddress property { selector, kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain };
        CFStringRef value = nullptr;
        UInt32 bytes = sizeof(value);
        if (AudioObjectGetPropertyData(device, &property, 0, nullptr, &bytes, &value) != noErr || !value) {
            return {};
        }
        const auto length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8) + 1;
        std::string text(static_cast<size_t>(length), '\0');
        const bool copied = CFStringGetCString(value, text.data(), length, kCFStringEncodingUTF8);
        CFRelease(value);
        if (!copied) {
            return {};
        }
        text.resize(std::strlen(text.c_str()));
        return text;
    }

    AudioDeviceID defaultDevice() {
        const AudioObjectPropertyAddress property { kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        AudioDeviceID device = kAudioObjectUnknown;
        UInt32 bytes = sizeof(device);
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &property, 0, nullptr, &bytes, &device);
        return device;
    }

    bool hasOutput(AudioDeviceID device) {
        const AudioObjectPropertyAddress property { kAudioDevicePropertyStreamConfiguration,
            kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain };
        UInt32 bytes = 0;
        if (AudioObjectGetPropertyDataSize(device, &property, 0, nullptr, &bytes) != noErr ||
            bytes < sizeof(AudioBufferList))
        {
            return false;
        }
        std::vector<unsigned char> storage(bytes);
        auto* buffers = reinterpret_cast<AudioBufferList*>(storage.data());
        if (AudioObjectGetPropertyData(device, &property, 0, nullptr, &bytes, buffers) != noErr) {
            return false;
        }
        for (UInt32 i = 0; i < buffers->mNumberBuffers; ++i) {
            if (buffers->mBuffers[i].mNumberChannels) {
                return true;
            }
        }
        return false;
    }

    mc_result visitDevice(AudioDeviceID device, mc_output_device_fn read, void* data) {
        if (device == kAudioObjectUnknown || !hasOutput(device)) {
            return MC_ERROR_NOT_FOUND;
        }
        const auto id = stringProperty(device, kAudioDevicePropertyDeviceUID);
        const auto name = stringProperty(device, kAudioObjectPropertyName);
        if (id.empty() || name.empty()) {
            return MC_ERROR_NOT_FOUND;
        }
        const mc_output_device item { { id.data(), id.size() }, { name.data(), name.size() } };
        read(data, &item);
        return MC_OK;
    }

    AudioDeviceID findDevice(mc_string_view id) {
        if (!id.length) {
            return defaultDevice();
        }
        const AudioObjectPropertyAddress property { kAudioHardwarePropertyTranslateUIDToDevice,
            kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        CFStringRef uid = CFStringCreateWithBytes(
            kCFAllocatorDefault, reinterpret_cast<const UInt8*>(id.data), id.length, kCFStringEncodingUTF8, false);
        if (!uid) {
            return kAudioObjectUnknown;
        }
        AudioDeviceID device = kAudioObjectUnknown;
        UInt32 bytes = sizeof(device);
        const auto result =
            AudioObjectGetPropertyData(kAudioObjectSystemObject, &property, sizeof(uid), &uid, &bytes, &device);
        CFRelease(uid);
        return result == noErr ? device : kAudioObjectUnknown;
    }
}

mc_result CoreAudioOut::Prepare(const mc_output_options& options, const mc_output_callbacks& callbacks) {
    if (findDevice(options.device_id) == kAudioObjectUnknown) {
        return MC_ERROR_NOT_FOUND;
    }
    this->format = options.format;
    this->callbacks = callbacks;
    this->capacity = options.queue_capacity_frames ? options.queue_capacity_frames : 32768;
    if (this->capacity > std::numeric_limits<UInt32>::max() / options.format.channels / sizeof(float)) {
        return MC_ERROR_BUFFER_TOO_SMALL;
    }
    const auto bytes = this->capacity * options.format.channels * sizeof(float);
    AudioStreamBasicDescription format {};
    format.mSampleRate = options.format.sample_rate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mFramesPerPacket = 1;
    format.mChannelsPerFrame = options.format.channels;
    format.mBitsPerChannel = 32;
    format.mBytesPerFrame = options.format.channels * sizeof(float);
    format.mBytesPerPacket = format.mBytesPerFrame;
    if (AudioQueueNewOutput(&format, Completed, this, nullptr, nullptr, 0, &this->queue) != noErr) {
        return MC_ERROR_IO;
    }
    if (AudioQueueAddPropertyListener(this->queue, kAudioQueueProperty_IsRunning, RunningChanged, this) != noErr) {
        return MC_ERROR_IO;
    }
    if (options.device_id.length) {
        CFStringRef uid =
            CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(options.device_id.data),
                options.device_id.length, kCFStringEncodingUTF8, false);
        if (!uid) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        const auto status = AudioQueueSetProperty(this->queue, kAudioQueueProperty_CurrentDevice, &uid, sizeof(uid));
        CFRelease(uid);
        if (status != noErr) {
            return MC_ERROR_IO;
        }
    }
    /* AudioQueue owns these buffers until synchronous disposal. no callback
    allocates, frees storage, or touches the engine's PCM buffer pool. */
    for (auto& slot : this->slots) {
        if (AudioQueueAllocateBuffer(this->queue, static_cast<UInt32>(bytes), &slot.buffer) != noErr) {
            return MC_ERROR_INTERNAL;
        }
        slot.buffer->mUserData = &slot;
    }
    if (this->SetVolume(options.initial_volume) != MC_OK || AudioQueueStart(this->queue, nullptr) != noErr) {
        return MC_ERROR_IO;
    }
    this->restartAfterDrain = false;
    this->paused.store(false);
    this->stopped.store(false);
    return MC_OK;
}

mc_result CoreAudioOut::QueueAudioBuffer(const mc_output_buffer& block) noexcept {
    /* Stop closes admission before waiting for producer calls, so a caller
    that arrives late cannot touch an AudioQueue being disposed. */
    this->producers.fetch_add(1);
    if (this->stopped.load()) {
        this->FinishProducer();
        return MC_ERROR_CANCELLED;
    }
    const auto& pcm = block.pcm;
    if (pcm.format.sample_rate != this->format.sample_rate || pcm.format.channels != this->format.channels) {
        this->FinishProducer();
        return MC_ERROR_INVALID_ARGUMENT;
    }
    if (pcm.frame_count > this->capacity) {
        this->FinishProducer();
        return MC_ERROR_BUFFER_TOO_SMALL;
    }
    if (this->queuedFrames.load() > this->capacity - pcm.frame_count) {
        this->FinishProducer();
        return MC_ERROR_QUEUE_FULL;
    }
    if (this->restartAfterDrain) {
        if (AudioQueueStart(this->queue, nullptr) != noErr) {
            this->FinishProducer();
            return MC_ERROR_IO;
        }
        this->restartAfterDrain = false;
    }
    for (auto& slot : this->slots) {
        if (slot.queued.load(std::memory_order_acquire)) {
            continue;
        }
        slot.token = block.token;
        slot.frames = pcm.frame_count;
        const auto bytes = pcm.frame_count * pcm.format.channels * sizeof(float);
        std::memcpy(slot.buffer->mAudioData, pcm.data, bytes);
        slot.buffer->mAudioDataByteSize = static_cast<UInt32>(bytes);
        this->queuedFrames.fetch_add(pcm.frame_count);
        slot.queued.store(true, std::memory_order_release);
        const auto result = AudioQueueEnqueueBuffer(this->queue, slot.buffer, 0, nullptr);
        if (result != noErr) {
            slot.queued.store(false, std::memory_order_release);
            this->queuedFrames.fetch_sub(pcm.frame_count);
        }
        this->FinishProducer();
        return result == noErr ? MC_OK : MC_ERROR_IO;
    }
    this->FinishProducer();
    return MC_ERROR_QUEUE_FULL;
}

void CoreAudioOut::Completed(void* state, AudioQueueRef, AudioQueueBufferRef buffer) noexcept {
    auto& output = *static_cast<CoreAudioOut*>(state);
    auto& slot = *static_cast<Slot*>(buffer->mUserData);
    if (!slot.queued.load(std::memory_order_acquire)) {
        return;
    }
    const auto token = slot.token;
    const auto frames = slot.frames;
    output.callbacks.completed(output.callbacks.context, token, output.stopped.load() ? MC_ERROR_CANCELLED : MC_OK);
    output.queuedFrames.fetch_sub(frames);
    slot.queued.store(false, std::memory_order_release);
    if (output.callbacks.ready) {
        output.callbacks.ready(output.callbacks.context);
    }
    output.NotifyDrainWaiter();
}

/* AudioQueue can call this on an audio thread. let Drain query the
driver on the playback worker. */
void CoreAudioOut::RunningChanged(void* state, AudioQueueRef, AudioQueuePropertyID) noexcept {
    static_cast<CoreAudioOut*>(state)->NotifyDrainWaiter();
}

void CoreAudioOut::FinishProducer() noexcept {
    this->producers.fetch_sub(1);
    this->producers.notify_all();
}

void CoreAudioOut::NotifyDrainWaiter() noexcept {
    this->drainNotifications.fetch_add(1);
    this->drainNotifications.notify_all();
}

mc_result CoreAudioOut::Stop() noexcept {
    this->stopped.store(true);
    this->NotifyDrainWaiter();
    auto active = this->producers.load();
    while (active) {
        this->producers.wait(active);
        active = this->producers.load();
    }
    if (!this->queue) {
        return MC_OK;
    }
    /* never hold a callback-visible lock while stopping/disposal synchronously
    waits for callbacks. disposal guarantees there are no later callbacks. */
    AudioQueueStop(this->queue, true);
    const auto disposed = AudioQueueDispose(this->queue, true);
    if (disposed != noErr) {
        return MC_ERROR_IO;
    }
    this->queue = nullptr;
    for (auto& slot : this->slots) {
        if (slot.queued.exchange(false)) {
            this->callbacks.completed(this->callbacks.context, slot.token, MC_ERROR_CANCELLED);
        }
        slot.buffer = nullptr;
    }
    this->queuedFrames.store(0);
    this->NotifyDrainWaiter();
    /* successful disposal establishes quiescence even when Stop reported a
    device that was already unavailable or a queue that never started. */
    return MC_OK;
}

mc_result CoreAudioOut::Pause() {
    if (!this->queue || AudioQueuePause(this->queue) != noErr) {
        return MC_ERROR_IO;
    }
    this->paused.store(true);
    this->NotifyDrainWaiter();
    return MC_OK;
}

mc_result CoreAudioOut::Resume() {
    if (!this->queue || AudioQueueStart(this->queue, nullptr) != noErr) {
        return MC_ERROR_IO;
    }
    this->restartAfterDrain = false;
    this->paused.store(false);
    this->NotifyDrainWaiter();
    return MC_OK;
}

mc_result CoreAudioOut::SetVolume(double value) {
    if (this->queue && AudioQueueSetParameter(this->queue, kAudioQueueParam_Volume, value) != noErr) {
        return MC_ERROR_IO;
    }
    this->volume.store(value);
    return MC_OK;
}

double CoreAudioOut::GetVolume() const {
    return this->volume.load();
}

mc_result CoreAudioOut::Drain() {
    if (this->stopped.load()) {
        return MC_ERROR_CANCELLED;
    }
    if (this->restartAfterDrain) {
        return MC_OK;
    }
    if (this->paused.load()) {
        return MC_ERROR_IN_FLIGHT;
    }
    /* buffer callbacks only prove storage is reusable. Stop(false) also plays
    driver-buffered audio before IsRunning changes to false. fence native calls
    against disposal, but release the fence while waiting so Stop can cancel. */
    this->producers.fetch_add(1);
    if (this->stopped.load()) {
        this->FinishProducer();
        return MC_ERROR_CANCELLED;
    }
    const auto requested = AudioQueueStop(this->queue, false);
    this->FinishProducer();
    if (requested != noErr) {
        return MC_ERROR_IO;
    }
    while (true) {
        /* take the count before checking the queue so a notification that
        arrives during the check also releases the wait below. */
        const auto observedNotifications = this->drainNotifications.load();
        this->producers.fetch_add(1);
        if (this->stopped.load()) {
            this->FinishProducer();
            return MC_ERROR_CANCELLED;
        }
        UInt32 running = 0;
        UInt32 size = sizeof(running);
        const auto status = AudioQueueGetProperty(this->queue, kAudioQueueProperty_IsRunning, &running, &size);
        this->FinishProducer();
        if (status != noErr) {
            return MC_ERROR_IO;
        }
        /* AudioQueue may still report its initial stopped state while startup
        is pending. accepted frames must finish as well as native playback. */
        if (!running && this->queuedFrames.load() == 0) {
            this->restartAfterDrain = true;
            return MC_OK;
        }
        this->drainNotifications.wait(observedNotifications);
    }
}

mc_result CoreAudioOut::VisitDevices(mc_output_device_fn read, void* data) {
    const AudioObjectPropertyAddress property { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain };
    UInt32 bytes = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &property, 0, nullptr, &bytes) != noErr) {
        return MC_ERROR_IO;
    }
    std::vector<AudioDeviceID> devices(bytes / sizeof(AudioDeviceID));
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &property, 0, nullptr, &bytes, devices.data()) != noErr) {
        return MC_ERROR_IO;
    }
    for (const auto device : devices) {
        visitDevice(device, read, data);
    }
    return MC_OK;
}

mc_result CoreAudioOut::ReadDefaultDevice(mc_output_device_fn read, void* data) {
    return visitDevice(defaultDevice(), read, data);
}

mc_result CoreAudioOut::GetDefaultFormat(mc_string_view id, mc_audio_format& format) {
    const auto device = findDevice(id);
    if (device == kAudioObjectUnknown) {
        return MC_ERROR_NOT_FOUND;
    }
    const AudioObjectPropertyAddress property { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain };
    Float64 rate = 0;
    UInt32 bytes = sizeof(rate);
    if (AudioObjectGetPropertyData(device, &property, 0, nullptr, &bytes, &rate) != noErr || !std::isfinite(rate) ||
        rate < 1 || rate > UINT32_MAX)
    {
        return MC_ERROR_IO;
    }
    format = { sizeof(format), static_cast<uint32_t>(rate), 2, 0 };
    return MC_OK;
}
