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

/* each prepared output has one worker that owns its COM apartment and WASAPI
interfaces. the host's producer and control threads submit one request at a
time and wait for that worker to finish it. WASAPI owns the device buffer, so
the worker copies PCM directly into its available space instead of keeping a
separate ring of host buffers. the producer lends each buffer until the worker
has copied all its frames and calls completed. the device may still be playing
that copy after the host reuses its buffer. a partial write that is cancelled
or fails returns the accepted token with that outcome.

when a write or drain needs device space, the worker waits for either WASAPI's
audio event or a stop event. Stop() closes admission and signals that event before waiting for
the worker to return the current request, finish callbacks, and release the
device. destruction sends an exit request and joins the worker. Windows device
notifications wake a stalled device wait; the worker checks the device and
handles a default-device change on a later write. While audio remains queued,
the worker also observes audio events between requests to update its estimated
played-frame count. GetTiming() reads the last atomic snapshot without waiting. */

#include "WasapiOutput.h"
#include "WasapiDevices.h"
#include "WasapiSettings.h"

#include <musikcore/plugin/services.h>
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <ks.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <intrin.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>

using Microsoft::WRL::ComPtr;

namespace {
    class NotificationClient final : public IMMNotificationClient {
    public:
        NotificationClient() : event(CreateEventW(nullptr, FALSE, FALSE, nullptr)) {}
        ~NotificationClient() {
            if (this->event && !CloseHandle(this->event)) {
                std::fputs("WASAPI output: cannot close device notification event\n", stderr);
                std::abort();
            }
        }

        ULONG STDMETHODCALLTYPE AddRef() override { return ++this->references; }
        ULONG STDMETHODCALLTYPE Release() override {
            const ULONG remaining = --this->references;
            if (!remaining) {
                delete this;
            }
            return remaining;
        }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** result) override {
            if (id == IID_IUnknown || id == __uuidof(IMMNotificationClient)) {
                *result = static_cast<IMMNotificationClient*>(this);
                this->AddRef();
                return S_OK;
            }
            *result = nullptr;
            return E_NOINTERFACE;
        }
        HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
            if (flow == eRender && role == eMultimedia) {
                this->changed.store(true);
                this->Wake();
            }
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { this->Wake(); return S_OK; }
        HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { this->Wake(); return S_OK; }
        HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

        /* the callback records a default-device change for the worker to consume. */
        std::atomic<bool> changed { false };

        /* wakes the worker when a device change may invalidate its audio event. */
        HANDLE event = nullptr;
    private:
        void Wake() noexcept {
            if (!SetEvent(this->event)) {
                std::fputs("WASAPI output: cannot signal device notification\n", stderr);
                std::abort();
            }
        }

        std::atomic<ULONG> references { 1 };
    };
}

struct WasapiOutput::State {
    /* Publish the estimated played-frame count and its timestamp in
    one 128-bit operation so concurrent readers always get the last complete
    reading without waiting for the COM worker. The high 16 bits of the frame
    word hold a nonzero generation; 48 frame bits cover over 180 years at 48 kHz. */
    struct alignas(16) TimingSnapshot {
        void Publish(const mc_output_timing& value) noexcept {
            constexpr uint64_t frameMask = (uint64_t { 1 } << 48) - 1;
            uint64_t packed = 0;
            uint64_t timestamp = 0;
            if ((value.valid_flags & MC_OUTPUT_TIMING_PLAYED) && value.generation &&
                value.generation <= UINT16_MAX && value.frames_played_by_driver <= frameMask)
            {
                packed = (value.generation << 48) | value.frames_played_by_driver;
                timestamp = value.frames_played_timestamp_ns;
            }
            alignas(16) LONG64 expected[2] { this->published[0], this->published[1] };
            if (!_InterlockedCompareExchange128(this->words, static_cast<LONG64>(timestamp),
                    static_cast<LONG64>(packed), expected))
            {
                std::fputs("WASAPI output: timing snapshot has multiple writers\n", stderr);
                std::abort();
            }
            this->published[0] = static_cast<LONG64>(packed);
            this->published[1] = static_cast<LONG64>(timestamp);
        }

        void Read(mc_output_timing& value) const noexcept {
            alignas(16) LONG64 words[2] {};
            _InterlockedCompareExchange128(const_cast<volatile LONG64*>(this->words), 0, 0, words);
            const auto packed = static_cast<uint64_t>(words[0]);
            if (packed >> 48) {
                value.valid_flags = MC_OUTPUT_TIMING_PLAYED | MC_OUTPUT_TIMING_ESTIMATED;
                value.generation = packed >> 48;
                value.frames_played_by_driver = packed & ((uint64_t { 1 } << 48) - 1);
                value.frames_played_timestamp_ns = static_cast<uint64_t>(words[1]);
            }
        }

        alignas(16) volatile LONG64 words[2] {};
        /* only the COM worker writes this copy. */
        LONG64 published[2] {};
    };

    enum class Command { Prepare, Queue, Pause, Resume, Volume, Drain, Stop, Exit };
    struct Request {
        Command command;
        const mc_output_options* options = nullptr;
        const mc_output_callbacks* callbacks = nullptr;
        const mc_output_buffer* buffer = nullptr;
        double number = 0;
        bool routing = false;
        mc_result result = MC_ERROR_INTERNAL;
        bool done = false;
    };

    struct Synchronization {
        /* serializes request submission and completion between host threads and the worker. */
        std::mutex mutex;

        /* wakes the worker for a request and callers when their request finishes. */
        std::condition_variable changed;

        /* closes admission before Stop() waits for the worker. */
        std::atomic<bool> stopping { true };

        /* manual-reset event interrupts a device wait, including while draining. */
        HANDLE stopEvent = nullptr;

        /* auto-reset event is signaled by WASAPI when the device buffer needs attention. */
        HANDLE audioEvent = nullptr;

        /* wakes the worker for a request while it watches the audio event. */
        HANDLE requestEvent = nullptr;
    };

    State() {
        this->sync.stopEvent = CreateEventW(nullptr, TRUE, TRUE, nullptr);
        if (!this->sync.stopEvent) {
            throw std::runtime_error("cannot create WASAPI stop event");
        }
        this->sync.requestEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!this->sync.requestEvent) {
            CloseHandle(this->sync.stopEvent);
            throw std::runtime_error("cannot create WASAPI request event");
        }
    }
    ~State() {
        if (this->worker.joinable()) {
            Request request { Command::Exit };
            this->Submit(request);
            this->worker.join();
        }
        if (!CloseHandle(this->sync.requestEvent) || !CloseHandle(this->sync.stopEvent)) {
            std::fputs("WASAPI output: cannot close stop event\n", stderr);
            std::abort();
        }
    }

    mc_result Submit(Request& request) {
        std::unique_lock lock(this->sync.mutex);
        if (!this->worker.joinable()) {
            if (request.command == Command::Stop || request.command == Command::Exit) {
                return MC_OK;
            }
            this->worker = std::thread([this] { this->Run(); });
        }
        this->sync.changed.wait(lock, [this] { return this->pending == nullptr; });
        this->pending = &request;
        if (!SetEvent(this->sync.requestEvent)) {
            std::abort();
        }
        this->sync.changed.notify_all();
        this->sync.changed.wait(lock, [&request] { return request.done; });
        return request.result;
    }

    mc_result WaitForAudio() {
        /* stop wins if both events are signaled. its handle remains alive
        until the worker has joined, including during device replacement. */
        const HANDLE events[] { this->sync.stopEvent, this->sync.audioEvent, this->notification->event };
        const DWORD result = WaitForMultipleObjects(3, events, FALSE, INFINITE);
        if (result == WAIT_OBJECT_0) {
            return MC_ERROR_CANCELLED;
        }
        if (result == WAIT_OBJECT_0 + 2) {
            return MC_OK;
        }
        return result == WAIT_OBJECT_0 + 1 ? MC_OK : MC_ERROR_IO;
    }

    void Close() {
        const mc_output_timing unknown = MC_OUTPUT_TIMING_DEFAULT;
        this->timing.Publish(unknown);
        if (this->notification && this->enumerator) {
            if (FAILED(this->enumerator->UnregisterEndpointNotificationCallback(this->notification.Get()))) {
                std::fputs("WASAPI output: cannot unregister device notification\n", stderr);
                std::abort();
            }
        }
        this->notification.Reset();
        this->enumerator.Reset();
        if (this->client) {
            this->client->Stop();
            this->client->Reset();
        }
        this->streamVolume.Reset();
        this->render.Reset();
        this->client.Reset();
        if (this->sync.audioEvent) {
            if (!CloseHandle(this->sync.audioEvent)) {
                std::fputs("WASAPI output: cannot close audio event\n", stderr);
                std::abort();
            }
            this->sync.audioEvent = nullptr;
        }
        this->paused = false;
    }

    mc_result SetVolume(double value) {
        this->volume.store(value);
        if (!this->streamVolume) {
            return MC_OK;
        }
        UINT32 channels = 0;
        if (FAILED(this->streamVolume->GetChannelCount(&channels))) {
            return MC_ERROR_IO;
        }
        for (UINT32 i = 0; i < channels; ++i) {
            if (FAILED(this->streamVolume->SetChannelVolume(i, static_cast<float>(value)))) {
                return MC_ERROR_IO;
            }
        }
        return MC_OK;
    }

    mc_result Prepare(const Request& request) {
        if (request.options != &this->savedOptions) {
            try {
                this->savedId.assign(request.options->device_id.data ? request.options->device_id.data : "",
                    request.options->device_id.length);
            }
            catch (...) {
                return MC_ERROR_INTERNAL;
            }
            this->savedOptions = *request.options;
            this->savedOptions.device_id = { this->savedId.data(), this->savedId.size() };
            this->savedCallbacks = *request.callbacks;
            this->routing = request.routing;
        }
        this->Close();
        const auto& options = *request.options;
        if (options.format.channels > USHRT_MAX / sizeof(float) ||
            options.format.sample_rate > UINT_MAX / (options.format.channels * sizeof(float)))
        {
            return MC_ERROR_UNSUPPORTED;
        }
        auto result = wasapi::OpenClient(options.device_id, this->client.GetAddressOf());
        if (result != MC_OK) {
            return result;
        }
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                IID_PPV_ARGS(this->enumerator.GetAddressOf()))))
        {
            return MC_ERROR_IO;
        }
        this->notification.Attach(new (std::nothrow) NotificationClient());
        if (!this->notification || !this->notification->event ||
            FAILED(this->enumerator->RegisterEndpointNotificationCallback(this->notification.Get())))
        {
            this->notification.Reset();
            return MC_ERROR_IO;
        }
        WAVEFORMATEXTENSIBLE format {};
        format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        format.Format.nChannels = static_cast<WORD>(options.format.channels);
        format.Format.nSamplesPerSec = options.format.sample_rate;
        format.Format.wBitsPerSample = 32;
        format.Format.nBlockAlign = static_cast<WORD>(options.format.channels * sizeof(float));
        format.Format.nAvgBytesPerSec = format.Format.nSamplesPerSec * format.Format.nBlockAlign;
        format.Samples.wValidBitsPerSample = 32;
        format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        switch (options.format.channels) {
            case 1:
                format.dwChannelMask = KSAUDIO_SPEAKER_MONO;
                break;
            case 2:
                format.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
                break;
            case 4:
                format.dwChannelMask = KSAUDIO_SPEAKER_QUAD;
                break;
            case 5:
                format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER |
                    SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
                break;
            case 6:
                format.dwChannelMask = KSAUDIO_SPEAKER_5POINT1;
                break;
            default:
                break;
        }
        WAVEFORMATEX* closest = nullptr;
        const HRESULT supported = this->client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &format.Format, &closest);
        CoTaskMemFree(closest);
        DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
        if (supported != S_OK) {
            flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        }
        this->sync.audioEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!this->sync.audioEvent ||
            FAILED(this->client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 0, 0, &format.Format, nullptr)) ||
            FAILED(this->client->SetEventHandle(this->sync.audioEvent)) ||
            FAILED(this->client->GetBufferSize(&this->capacity)) ||
            FAILED(this->client->GetService(IID_PPV_ARGS(this->render.GetAddressOf()))) ||
            FAILED(this->client->GetService(IID_PPV_ARGS(this->streamVolume.GetAddressOf()))))
        {
            return MC_ERROR_IO;
        }
        this->format = options.format;
        this->queueLimit = options.queue_capacity_frames
            ? static_cast<UINT32>(std::min<size_t>(this->capacity, options.queue_capacity_frames))
            : this->capacity;
        this->callbacks = *request.callbacks;
        this->submittedFrames = 0;
        this->timingPending = false;
        this->generation = this->generation == UINT16_MAX ? 1 : this->generation + 1;
        result = this->SetVolume(options.initial_volume);
        if (result != MC_OK || FAILED(this->client->Start())) {
            return MC_ERROR_IO;
        }
        this->UpdateTiming();
        return MC_OK;
    }

    void UpdateTiming() noexcept {
        UINT32 padding = 0;
        if (!this->client || FAILED(this->client->GetCurrentPadding(&padding)) || padding > this->submittedFrames) {
            return;
        }
        const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (now <= 0) {
            return;
        }
        this->timingPending = padding != 0;
        mc_output_timing snapshot = MC_OUTPUT_TIMING_DEFAULT;
        /* Padding counts frames not yet consumed by the audio engine. Some
        consumed frames may still be in the device pipeline, so this is an estimate. */
        snapshot.valid_flags = MC_OUTPUT_TIMING_PLAYED | MC_OUTPUT_TIMING_ESTIMATED;
        snapshot.generation = this->generation;
        snapshot.frames_played_by_driver = this->submittedFrames - padding;
        snapshot.frames_played_timestamp_ns = static_cast<uint64_t>(now);
        this->timing.Publish(snapshot);
    }

    mc_result Queue(const mc_output_buffer& buffer) {
        if (!this->client || this->sync.stopping.load()) {
            return MC_ERROR_CANCELLED;
        }

        if (buffer.pcm.format.sample_rate != this->format.sample_rate ||
            buffer.pcm.format.channels != this->format.channels ||
            buffer.pcm.format.channel_layout != this->format.channel_layout)
        {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        size_t written = 0;
        mc_result outcome = MC_OK;
        bool retriedInvalidation = false;

        while (written < buffer.pcm.frame_count) {
            if (this->sync.stopping.load()) {
                outcome = MC_ERROR_CANCELLED;
                break;
            }

            /* Keep paused audio on its current endpoint until it can drain. */
            if (!this->paused && this->routing && !this->savedOptions.device_id.length &&
                this->notification->changed.exchange(false)) {
                /* Finish queued audio on the old endpoint when possible. A
                removed endpoint may fail; then reopening is the only option. */
                this->Drain();
                if (this->sync.stopping.load()) {
                    outcome = MC_ERROR_CANCELLED;
                    break;
                }

                Request reopen { Command::Prepare };
                this->savedOptions.initial_volume = this->volume.load();
                reopen.options = &this->savedOptions;
                reopen.callbacks = &this->savedCallbacks;
                outcome = this->Prepare(reopen);

                if (outcome != MC_OK) {
                    break;
                }
            }

            UINT32 padding = 0;
            const HRESULT paddingResult = this->client->GetCurrentPadding(&padding);

            if (paddingResult == AUDCLNT_E_DEVICE_INVALIDATED && !retriedInvalidation &&
                !this->paused && this->routing && !this->savedOptions.device_id.length) {
                /* Invalidation can precede the default-change callback. Try
                the current default once without waiting for that callback. */
                retriedInvalidation = true;
                Request reopen { Command::Prepare };
                this->savedOptions.initial_volume = this->volume.load();
                reopen.options = &this->savedOptions;
                reopen.callbacks = &this->savedCallbacks;
                outcome = this->Prepare(reopen);

                if (outcome == MC_OK) {
                    continue;
                }
                break;
            }

            if (FAILED(paddingResult) || padding > this->capacity) {
                outcome = MC_ERROR_IO;
                break;
            }

            const auto available = padding < this->queueLimit ? this->queueLimit - padding : 0;
            this->UpdateTiming();

            /* A paused device cannot free space until the host resumes it. */
            if (this->paused && !written && buffer.pcm.frame_count > this->queueLimit) {
                return MC_ERROR_BUFFER_TOO_SMALL;
            }
            if (this->paused && !written && buffer.pcm.frame_count > available) {
                return MC_ERROR_QUEUE_FULL;
            }

            /* Wait for space or cancellation before attempting the next copy. */
            if (!available) {
                if (this->paused && !written) {
                    return MC_ERROR_QUEUE_FULL;
                }

                outcome = this->WaitForAudio();
                if (outcome != MC_OK) {
                    break;
                }
                continue;
            }

            const auto frames = static_cast<UINT32>(std::min<size_t>(available, buffer.pcm.frame_count - written));
            BYTE* destination = nullptr;
            if (FAILED(this->render->GetBuffer(frames, &destination))) {
                outcome = MC_ERROR_IO;
                break;
            }

            std::memcpy(destination, buffer.pcm.data + written * this->format.channels,
                static_cast<size_t>(frames) * this->format.channels * sizeof(float));

            if (FAILED(this->render->ReleaseBuffer(frames, 0))) {
                outcome = MC_ERROR_IO;
                break;
            }

            written += frames;
            this->submittedFrames += frames;
            this->UpdateTiming();
        }

        /* An untouched buffer remains with the caller. Once any frames were
        copied, return its token through completed, even on cancellation. */
        if (!written && outcome != MC_OK) {
            return outcome;
        }

        this->callbacks.completed(this->callbacks.context, buffer.token, outcome);
        return MC_OK;
    }

    mc_result Drain() {
        while (!this->sync.stopping.load()) {
            UINT32 padding = 0;
            if (!this->client || FAILED(this->client->GetCurrentPadding(&padding))) {
                return MC_ERROR_IO;
            }
            if (!padding) {
                this->UpdateTiming();
                return MC_OK;
            }
            if (this->paused) {
                return MC_ERROR_IN_FLIGHT;
            }
            const auto result = this->WaitForAudio();
            if (result != MC_OK) {
                return result;
            }
        }
        return MC_ERROR_CANCELLED;
    }

    mc_result Execute(Request& request) {
        switch (request.command) {
            case Command::Prepare: return this->Prepare(request);
            case Command::Queue: return this->Queue(*request.buffer);
            case Command::Pause:
                if (!this->client || FAILED(this->client->Stop())) {
                    return MC_ERROR_IO;
                }
                this->paused = true;
                this->UpdateTiming();
                return MC_OK;
            case Command::Resume:
                if (!this->client || FAILED(this->client->Start())) {
                    return MC_ERROR_IO;
                }
                this->paused = false;
                this->UpdateTiming();
                if (this->callbacks.ready) {
                    this->callbacks.ready(this->callbacks.context);
                }
                return MC_OK;
            case Command::Volume: return this->SetVolume(request.number);
            case Command::Drain: return this->Drain();
            case Command::Stop: this->Close(); return MC_OK;
            case Command::Exit: this->Close(); return MC_OK;
        }
        return MC_ERROR_INTERNAL;
    }

    void Run() noexcept {
        const HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        for (;;) {
            const HANDLE events[] { this->sync.requestEvent, this->sync.audioEvent };
            const DWORD count = this->timingPending && !this->paused && this->sync.audioEvent ? 2 : 1;
            const DWORD wake = WaitForMultipleObjects(count, events, FALSE, INFINITE);
            if (wake == WAIT_OBJECT_0 + 1) {
                this->UpdateTiming();
                continue;
            }
            if (wake != WAIT_OBJECT_0) {
                std::abort();
            }
            std::unique_lock lock(this->sync.mutex);
            Request* request = this->pending;
            if (!request) {
                continue;
            }
            lock.unlock();
            const auto result = SUCCEEDED(apartment) || request->command == Command::Stop || request->command == Command::Exit
                ? this->Execute(*request) : MC_ERROR_IO;
            lock.lock();
            const bool quitting = request->command == Command::Exit;
            request->result = result;
            request->done = true;
            this->pending = nullptr;
            this->sync.changed.notify_all();
            if (quitting) {
                break;
            }
        }
        if (SUCCEEDED(apartment)) {
            CoUninitialize();
        }
    }

    Synchronization sync;
    /* protected by sync.mutex; the submitting caller owns it until completion. */
    Request* pending = nullptr;
    /* volume is read by control callers while the worker applies changes. */
    std::atomic<double> volume { 1.0 };
    TimingSnapshot timing;
    ComPtr<IAudioClient> client;
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<NotificationClient> notification;
    ComPtr<IAudioRenderClient> render;
    ComPtr<IAudioStreamVolume> streamVolume;
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    mc_output_options savedOptions = MC_OUTPUT_OPTIONS_DEFAULT;
    mc_output_callbacks savedCallbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    std::string savedId;
    bool routing = false;
    UINT32 capacity = 0;
    UINT32 queueLimit = 0;
    uint64_t submittedFrames = 0;
    bool timingPending = false;
    uint64_t generation = 0;
    bool paused = false;
    std::thread worker;
};

WasapiOutput::WasapiOutput(const mc_plugin_services* services)
: state(std::make_unique<State>()), services(services) {}

WasapiOutput::~WasapiOutput() noexcept = default;

mc_result WasapiOutput::Prepare(const mc_output_options& options, const mc_output_callbacks& callbacks) {
    bool routing = wasapi::EndpointRouting.default_value;
    if (this->services && this->services->preferences) {
        const auto* preferences = this->services->preferences;
        const auto routeResult = preferences->read_boolean(preferences->context, wasapi::EndpointRouting.key,
            routing, &routing);
        if (routeResult != MC_OK) {
            return routeResult;
        }
    }
    if (!ResetEvent(this->state->sync.stopEvent)) {
        return MC_ERROR_IO;
    }
    this->state->sync.stopping.store(false);
    State::Request request { State::Command::Prepare };
    request.options = &options;
    request.callbacks = &callbacks;
    request.routing = routing;
    return this->state->Submit(request);
}

mc_result WasapiOutput::QueueAudioBuffer(const mc_output_buffer& buffer) noexcept {
    State::Request request { State::Command::Queue };
    request.buffer = &buffer;
    return this->state->Submit(request);
}

mc_result WasapiOutput::Pause() { State::Request request { State::Command::Pause }; return this->state->Submit(request); }
mc_result WasapiOutput::Resume() { State::Request request { State::Command::Resume }; return this->state->Submit(request); }
mc_result WasapiOutput::SetVolume(double value) {
    State::Request request { State::Command::Volume };
    request.number = value;
    return this->state->Submit(request);
}
double WasapiOutput::GetVolume() const { return this->state->volume.load(); }
mc_result WasapiOutput::Stop() noexcept {
    this->state->sync.stopping.store(true);
    if (!SetEvent(this->state->sync.stopEvent)) {
        std::fputs("WASAPI output: cannot signal stop event\n", stderr);
        std::abort();
    }
    State::Request request { State::Command::Stop };
    return this->state->Submit(request);
}
mc_result WasapiOutput::Drain() { State::Request request { State::Command::Drain }; return this->state->Submit(request); }
mc_result WasapiOutput::GetTiming(mc_output_timing& timing) const noexcept {
    this->state->timing.Read(timing);
    return MC_OK;
}
mc_result WasapiOutput::VisitDevices(mc_output_device_fn read, void* user) { return wasapi::VisitDevices(read, user); }
mc_result WasapiOutput::ReadDefaultDevice(mc_output_device_fn read, void* user) { return wasapi::ReadDefaultDevice(read, user); }
mc_result WasapiOutput::GetDefaultFormat(mc_string_view id, mc_audio_format& format) {
    return wasapi::GetDefaultFormat(this->services, id, format);
}
