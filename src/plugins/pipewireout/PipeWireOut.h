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

#include <musikcore/sdk/IOutput.h>
#include <pipewire/pipewire.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <deque>
#include <vector>
#include <condition_variable>

using namespace musik::core::sdk;

class PipeWireOut : public IOutput {
    public:
        PipeWireOut();
        ~PipeWireOut();

        /* IPlugin */
        const char* Name() override { return "PipeWire"; };
        void Release() override;

        /* IOutput */
        void Pause() override;
        void Resume() override;
        void SetVolume(double volume) override;
        double GetVolume() override;
        void Stop() override;
        OutputState Play(IBuffer *buffer, IBufferProvider *provider) override;
        double Latency() override;
        void Drain() override;
        IDeviceList* GetDeviceList() override;
        bool SetDefaultDevice(const char* deviceId) override;
        IDevice* GetDefaultDevice() override;
        int GetDefaultSampleRate() override { return -1; }

    private:
        bool StartPipeWire(IBuffer* buffer);
        void StopPipeWire();
        void DiscardInputBuffers();
        void RefreshDeviceList();

        static void OnCoreDone(
            void* userdata,
            uint32_t id,
            int seq);

        static void OnCoreError(
            void *userdata,
            uint32_t id,
            int seq,
            int res,
            const char *message
        );

        static void OnRegistryGlobal(
            void *userdata,
            uint32_t id,
			uint32_t permissions,
            const char *type,
            uint32_t version,
			const struct spa_dict *props);

        static void OnStreamStateChanged(
            void* userdata,
            enum pw_stream_state old,
            enum pw_stream_state state,
            const char* error);

        static void OnStreamProcess(void* userdata);

        static void OnDrained(void* userdata);

        struct InBufferContext {
            InBufferContext(IBuffer* buffer, IBufferProvider* provider) {
                this->buffer = buffer; this->provider = provider;
                this->readPtr = (char*) buffer->BufferPointer();
                this->remaining = (uint32_t) buffer->Bytes();
            }
            void Advance(int count) {
                bool release = count >= remaining;
                this->remaining -= count;
                this->readPtr += count;
                if (release) {
                    this->provider->OnBufferProcessed(this->buffer);
                    delete this;
                }
            }
            void Discard() {
                this->provider->OnBufferProcessed(this->buffer);
                delete this;
            }
            IBuffer* buffer{nullptr};
            IBufferProvider* provider{nullptr};
            uint32_t remaining{0};
            char* readPtr;
        };

        struct OutBufferContext {
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
            void Reset() {
                this->buffer = nullptr;
                this->writePtr = nullptr;
                this->remaining = 0;
                this->total = 0;
            }
            void Advance(int count) {
                this->remaining -= count;
                this->writePtr += count;
            }
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
            bool Valid() {
                return this->buffer != nullptr;
            }
            pw_buffer* buffer{nullptr};
            uint32_t remaining{0};
            uint32_t total{0};
            char* writePtr{nullptr};
        };

        class Device: public musik::core::sdk::IDevice {
            public:
                Device(const std::string& id, const std::string& name) {
                    this->id = id;
                    this->name = name;
                }
                void Release() override {
                    delete this;
                }
                const char* Name() const override {
                    return name.c_str();
                }
                const char* Id() const override {
                    return id.c_str();
                }
                Device* Clone() {
                    return new Device(this->id, this->name);
                }
            private:
                std::string id, name;
        };

        class DeviceList: public musik::core::sdk::IDeviceList {
            public:
                void Release() override {
                    delete this;
                }
                size_t Count() const override {
                    return devices.size();
                }
                const Device* At(size_t index) const override {
                    return &devices.at(index);
                }
                void Add(const std::string& id, const std::string& name) {
                    devices.push_back(Device(id, name));
                }
                Device* Default() {
                    return this->devices.empty() ? nullptr : this->devices.at(0).Clone();
                }
                void Reset() {
                    this->devices.clear();
                }
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
                Device* ResolveDevice(const std::string& id) {
                    for (auto device: this->devices) {
                        if (device.Id() == id) {
                            return device.Clone();
                        }
                    }
                    return nullptr;
                }
                DeviceList* Clone() {
                    auto result = new DeviceList();
                    result->devices = this->devices;
                    return result;
                }
            private:
                std::vector<Device> devices;
        };

        struct DeviceListContext {
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
            pw_main_loop* loop{nullptr};
            pw_context* context{nullptr};
            pw_core* core{nullptr};
            spa_hook coreListener;
            pw_core_events coreEvents;
            pw_registry* registry{nullptr};
            spa_hook registryListener;
            pw_registry_events registryEvents;
            int eventId{0};
            PipeWireOut* instance{nullptr};
        };

        enum class State {
            Stopped, Paused, Playing, Shutdown
        };

        std::deque<InBufferContext*> buffers;
        std::recursive_mutex mutex;
        std::atomic<bool> initialized{false};
        std::atomic<State> state{State::Stopped};
        std::condition_variable_any bufferCondition, drainCondition;
        double volume{1.0};
        pw_stream_events pwStreamEvents;
        pw_thread_loop* pwThreadLoop{nullptr};
        pw_stream* pwStream{nullptr};
        OutBufferContext outBufferContext;
        long channelCount{0};
        long sampleRate{0};
        size_t maxInternalBuffers{0};
        DeviceList deviceList;
};
