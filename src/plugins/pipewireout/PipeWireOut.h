//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

#pragma once

/// @file PipeWireOut.h
/// @brief Audio output plugin built on the PipeWire multimedia framework.
/// @details Implements the output SDK on top of PipeWire streams. Audio is
/// pushed into pw_stream buffers from a background thread, driven by the
/// stream process callback. Supports device enumeration via the registry,
/// volume control, pause/resume, stop and drain operations. Linux-only.

#include <musikcore/sdk/IOutput.h>
#include <pipewire/pipewire.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <deque>
#include <vector>
#include <condition_variable>

using namespace musik::core::sdk;

/** @brief PipeWire-based output for Linux.
 *  @details Creates a pw_stream that negotiates format with a PipeWire sink.
 *  Incoming PCM buffers are queued and copied into PipeWire stream buffers as
 *  they become available. Device enumeration walks the PipeWire registry and
 *  exposes the discovered sink nodes through the SDK device interfaces. */
class PipeWireOut : public IOutput {
    public:
        /** @brief Constructs an uninitialized output. */
        PipeWireOut();
        /** @brief Destroys the output and stops PipeWire. */
        ~PipeWireOut();

        /* IPlugin */
        /** @brief Returns the plugin name.
         *  @return Always returns "PipeWire". */
        const char* Name() override { return "PipeWire"; };
        /** @brief Destroys the output instance and releases PipeWire resources. */
        void Release() override;

        /* IOutput */
        /** @brief Pauses audio playback. */
        void Pause() override;
        /** @brief Resumes audio playback after a pause. */
        void Resume() override;
        /** @brief Sets the playback volume.
         *  @param volume Volume in the range 0.0 (mute) to 1.0 (full). */
        void SetVolume(double volume) override;
        /** @brief Returns the current playback volume.
         *  @return Volume in the range 0.0 to 1.0. */
        double GetVolume() override;
        /** @brief Stops playback and discards queued buffers. */
        void Stop() override;
        /** @brief Queues a buffer for playback.
         *  @param buffer The PCM buffer to play.
         *  @param provider The provider that owns the buffer.
         *  @return An OutputState indicating whether the output is playing or paused. */
        OutputState Play(IBuffer *buffer, IBufferProvider *provider) override;
        /** @brief Returns the output latency in seconds.
         *  @return The measured latency in seconds. */
        double Latency() override;
        /** @brief Blocks until all queued audio has been written to the device. */
        void Drain() override;
        /** @brief Returns the list of available PipeWire devices.
         *  @return A newly allocated IDeviceList the caller must release. */
        IDeviceList* GetDeviceList() override;
        /** @brief Selects the default output device.
         *  @param deviceId Identifier of the device to use.
         *  @return True if the device was selected successfully. */
        bool SetDefaultDevice(const char* deviceId) override;
        /** @brief Returns the default output device.
         *  @return The currently configured default device. */
        IDevice* GetDefaultDevice() override;
        /** @brief Returns the preferred sample rate.
         *  @return Always returns -1 (no preference). */
        int GetDefaultSampleRate() override { return -1; }

    private:
        /** @brief Initializes the PipeWire stream for a given buffer format.
         *  @param buffer A buffer whose format will be negotiated.
         *  @return True if PipeWire initialized successfully. */
        bool StartPipeWire(IBuffer* buffer);
        /** @brief Destroys the PipeWire stream and thread loop. */
        void StopPipeWire();
        /** @brief Releases all queued input buffers back to their providers. */
        void DiscardInputBuffers();
        /** @brief Re-enumerates the sink devices from the registry. */
        void RefreshDeviceList();

        /** @brief PipeWire core done callback.
         *  @param userdata The PipeWireOut instance.
         *  @param id Object id of the completed operation.
         *  @param seq Sequence number of the operation. */
        static void OnCoreDone(
            void* userdata,
            uint32_t id,
            int seq);

        /** @brief PipeWire core error callback.
         *  @param userdata The PipeWireOut instance.
         *  @param id Object id related to the error.
         *  @param seq Sequence number related to the error.
         *  @param res Error code.
         *  @param message Human readable error message. */
        static void OnCoreError(
            void *userdata,
            uint32_t id,
            int seq,
            int res,
            const char *message
        );

        /** @brief PipeWire registry global callback discovering sink nodes.
         *  @param userdata The PipeWireOut instance.
         *  @param id Global id of the object.
         *  @param permissions Permissions of the object.
         *  @param type Object type name.
         *  @param version Object version.
         *  @param props Properties of the object. */
        static void OnRegistryGlobal(
            void *userdata,
            uint32_t id,
			uint32_t permissions,
            const char *type,
            uint32_t version,
			const struct spa_dict *props);

        /** @brief PipeWire stream state changed callback.
         *  @param userdata The PipeWireOut instance.
         *  @param old Previous stream state.
         *  @param state New stream state.
         *  @param error Error message, or null. */
        static void OnStreamStateChanged(
            void* userdata,
            enum pw_stream_state old,
            enum pw_stream_state state,
            const char* error);

        /** @brief PipeWire stream process callback.
         *  @details Called when output space is available; copies queued PCM
         *  into the stream buffer and queues it back to PipeWire.
         *  @param userdata The PipeWireOut instance. */
        static void OnStreamProcess(void* userdata);

        /** @brief PipeWire stream drained callback.
         *  @param userdata The PipeWireOut instance. */
        static void OnDrained(void* userdata);

        /** @brief Tracks the consumption of one queued input PCM buffer. */
        struct InBufferContext {
            /** @brief Wraps a buffer and its provider.
             *  @param buffer The input PCM buffer.
             *  @param provider The provider owning the buffer. */
            InBufferContext(IBuffer* buffer, IBufferProvider* provider) {
                this->buffer = buffer; this->provider = provider;
                this->readPtr = (char*) buffer->BufferPointer();
                this->remaining = (uint32_t) buffer->Bytes();
            }
            /** @brief Advances the read pointer by the given count.
             *  @details When the buffer is fully consumed it is released back
             *  to the provider and this context is deleted.
             *  @param count Number of bytes consumed. */
            void Advance(int count) {
                bool release = count >= remaining;
                this->remaining -= count;
                this->readPtr += count;
                if (release) {
                    this->provider->OnBufferProcessed(this->buffer);
                    delete this;
                }
            }
            /** @brief Discards the buffer without playing it. */
            void Discard() {
                this->provider->OnBufferProcessed(this->buffer);
                delete this;
            }
            /** @brief The input PCM buffer. */
            IBuffer* buffer{nullptr};
            /** @brief The provider owning the buffer. */
            IBufferProvider* provider{nullptr};
            /** @brief Bytes not yet consumed. */
            uint32_t remaining{0};
            /** @brief Current read position. */
            char* readPtr;
        };

        /** @brief Tracks the fill state of one PipeWire output buffer. */
        struct OutBufferContext {
            /** @brief Initializes the context from a PipeWire buffer.
             *  @param buffer The PipeWire buffer to fill. */
            void Initialize(pw_buffer* buffer) {
                this->buffer = buffer;
                if (buffer) {
                    struct spa_buffer* spaBuffer = buffer->buffer;
                    this->writePtr = (char*) spaBuffer->datas[0].data;
                    this->remaining = spaBuffer->datas[0].maxsize;
                    this->total = this->remaining;
                }
                else {
                    this->Reset();
                }
            }
            /** @brief Clears the context to an invalid state. */
            void Reset() {
                this->buffer = nullptr;
                this->writePtr = nullptr;
                this->remaining = 0;
                this->total = 0;
            }
            /** @brief Advances the write pointer by the given count.
             *  @param count Number of bytes written. */
            void Advance(int count) {
                this->remaining -= count;
                this->writePtr += count;
            }
            /** @brief Queues the filled buffer back to the PipeWire stream.
             *  @param stream The PipeWire stream.
             *  @param stride Bytes per sample frame. */
            void Finalize(pw_stream* stream, uint32_t stride) {
                if (this->Valid()) {
                    spa_data& data = this->buffer->buffer->datas[0];
                    data.chunk->offset = 0;
                    data.chunk->stride = stride;
                    data.chunk->size = this->total - this->remaining;
                    pw_stream_queue_buffer(stream, this->buffer);
                    this->Reset();
                }
            }
            /** @brief Returns whether a PipeWire buffer is attached.
             *  @return True if a buffer is being filled. */
            bool Valid() {
                return this->buffer != nullptr;
            }
            /** @brief The PipeWire buffer being filled. */
            pw_buffer* buffer{nullptr};
            /** @brief Bytes of space remaining in the buffer. */
            uint32_t remaining{0};
            /** @brief Total capacity of the buffer in bytes. */
            uint32_t total{0};
            /** @brief Current write position. */
            char* writePtr{nullptr};
        };

        /** @brief Describes a PipeWire sink device to the SDK. */
        class Device: public musik::core::sdk::IDevice {
            public:
                /** @brief Constructs a device description.
                 *  @param id The device id.
                 *  @param name The display name. */
                Device(const std::string& id, const std::string& name) {
                    this->id = id;
                    this->name = name;
                }
                /** @brief Destroys the device description. */
                void Release() override {
                    delete this;
                }
                /** @brief Returns the display name.
                 *  @return The device name. */
                const char* Name() const override {
                    return name.c_str();
                }
                /** @brief Returns the device id.
                 *  @return The device id. */
                const char* Id() const override {
                    return id.c_str();
                }
                /** @brief Clones the device description.
                 *  @return A new Device copy. */
                Device* Clone() {
                    return new Device(this->id, this->name);
                }
            private:
                /** @brief Device id and display name. */
                std::string id, name;
        };

        /** @brief List of PipeWire sink devices exposed to the SDK. */
        class DeviceList: public musik::core::sdk::IDeviceList {
            public:
                /** @brief Destroys the device list. */
                void Release() override {
                    delete this;
                }
                /** @brief Returns the number of devices.
                 *  @return Device count. */
                size_t Count() const override {
                    return devices.size();
                }
                /** @brief Returns the device at the given index.
                 *  @param index Zero-based index.
                 *  @return The device description. */
                const Device* At(size_t index) const override {
                    return &devices.at(index);
                }
                /** @brief Appends a device to the list.
                 *  @param id The device id.
                 *  @param name The display name. */
                void Add(const std::string& id, const std::string& name) {
                    devices.push_back(Device(id, name));
                }
                /** @brief Returns the default device.
                 *  @return A cloned first device, or null. */
                Device* Default() {
                    return this->devices.empty() ? nullptr : this->devices.at(0).Clone();
                }
                /** @brief Clears the device list. */
                void Reset() {
                    this->devices.clear();
                }
                /** @brief Resolves a device id string to a PipeWire node id.
                 *  @param id The device id.
                 *  @return The numeric node id, or PW_ID_ANY. */
                uint32_t ResolveId(const std::string& id) {
                    for (auto device: this->devices) {
                        if (device.Id() == id) {
                            try {
                                return (uint32_t) std::stoi(id);
                            }
                            catch(...) {
                                /* return default below... */
                            }
                        }
                    }
                    return PW_ID_ANY;
                }
                /** @brief Resolves a device id string to a device.
                 *  @param id The device id.
                 *  @return A cloned Device, or null. */
                Device* ResolveDevice(const std::string& id) {
                    for (auto device: this->devices) {
                        if (device.Id() == id) {
                            return device.Clone();
                        }
                    }
                    return nullptr;
                }
                /** @brief Clones the device list.
                 *  @return A new DeviceList copy. */
                DeviceList* Clone() {
                    auto result = new DeviceList();
                    result->devices = this->devices;
                    return result;
                }
            private:
                /** @brief The contained devices. */
                std::vector<Device> devices;
        };

        /** @brief State used while enumerating devices from the registry. */
        struct DeviceListContext {
            /** @brief Initializes the context with the owner instance.
             *  @param instance The PipeWireOut owning this context. */
            DeviceListContext(PipeWireOut* instance) {
                this->instance = instance;
                this->coreEvents = { PW_VERSION_CORE_EVENTS };
                this->coreEvents.done = PipeWireOut::OnCoreDone;
                this->coreEvents.error = PipeWireOut::OnCoreError;
                spa_zero(this->coreListener);
                this->registryEvents = { PW_VERSION_REGISTRY_EVENTS };
                this->registryEvents.global = PipeWireOut::OnRegistryGlobal;
                spa_zero(this->registryListener);
            }
            /** @brief Destroys the registry, context and loop. */
            ~DeviceListContext() {
                if (this->registry) {
                    pw_proxy_destroy(reinterpret_cast<pw_proxy*>(this->registry));
                }
                if (this->context) {
                    pw_context_destroy(this->context);
                }
                if (this->loop) {
                    pw_main_loop_destroy(this->loop);
                }
            }
            /** @brief Main loop driving the registry query. */
            pw_main_loop* loop{nullptr};
            /** @brief PipeWire context. */
            pw_context* context{nullptr};
            /** @brief PipeWire core connection. */
            pw_core* core{nullptr};
            /** @brief Hook for core events. */
            spa_hook coreListener;
            /** @brief Core event callbacks. */
            pw_core_events coreEvents;
            /** @brief PipeWire registry proxy. */
            pw_registry* registry{nullptr};
            /** @brief Hook for registry events. */
            spa_hook registryListener;
            /** @brief Registry event callbacks. */
            pw_registry_events registryEvents;
            /** @brief Last processed global id. */
            int eventId{0};
            /** @brief Owner of this context. */
            PipeWireOut* instance{nullptr};
        };

        /** @brief Playback state of the output. */
        enum class State {
            /** @brief Playback is stopped. */
            Stopped, /** @brief Playback is paused. */
            Paused, /** @brief Playback is active. */
            Playing, /** @brief The output is shutting down. */
            Shutdown
        };

        /** @brief Queued input buffers awaiting playback. */
        std::deque<InBufferContext*> buffers;
        /** @brief Guards the output state and queue. */
        std::recursive_mutex mutex;
        /** @brief Whether PipeWire has been initialized. */
        std::atomic<bool> initialized{false};
        /** @brief Current playback state. */
        std::atomic<State> state{State::Stopped};
        /** @brief Signals buffer availability and drain completion. */
        std::condition_variable_any bufferCondition, drainCondition;
        /** @brief Current playback volume, 0.0 to 1.0. */
        double volume{1.0};
        /** @brief PipeWire stream event callbacks. */
        pw_stream_events pwStreamEvents;
        /** @brief PipeWire thread loop for the stream. */
        pw_thread_loop* pwThreadLoop{nullptr};
        /** @brief PipeWire stream handle. */
        pw_stream* pwStream{nullptr};
        /** @brief State of the PipeWire output buffer. */
        OutBufferContext outBufferContext;
        /** @brief Number of interleaved channels. */
        long channelCount{0};
        /** @brief Sample rate in Hz. */
        long sampleRate{0};
        /** @brief Maximum number of internally queued buffers. */
        size_t maxInternalBuffers{0};
        /** @brief Enumerated sink devices. */
        DeviceList deviceList;
};
