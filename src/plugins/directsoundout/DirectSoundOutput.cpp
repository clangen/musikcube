#include "DirectSoundOutput.h"
#include "DirectSoundDevices.h"
#include <common/FixedRingQueue.h>

#define NOMINMAX
#include <windows.h>
#include <audioclient.h>
#include <dsound.h>
#include <ks.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <intrin.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
    /* Tracks audio ranges in DirectSound's looping buffer. Positions exposed
    by this class are absolute frames; only Lock sees wrapped byte offsets. */
    class DeviceRing {
    public:
        void Reset(DWORD frames, DWORD bytesPerFrame, DWORD safetyFrames) {
            this->frames = frames;
            this->bytesPerFrame = bytesPerFrame;
            this->safetyFrames = safetyFrames;
            this->lastCursor = 0;
            this->play = 0;
            this->tail = 0;
            this->segments.clear();
        }

        size_t Advance(DWORD playBytes) {
            const DWORD cursor = (playBytes / this->bytesPerFrame) % this->frames;
            const DWORD advance = (cursor + this->frames - this->lastCursor) % this->frames;
            this->lastCursor = cursor;
            const uint64_t previous = this->play;
            this->play += advance;

            size_t consumed = 0;
            while (!this->segments.empty()) {
                const auto& segment = this->segments.front();
                if (segment.begin >= this->play) {
                    break;
                }
                const auto end = std::min(segment.end, this->play);
                const auto begin = std::max(segment.begin, previous);
                if (end > begin) {
                    consumed += static_cast<size_t>(end - begin);
                }
                if (segment.end > this->play) {
                    break;
                }
                this->segments.pop_front();
            }
            return consumed;
        }

        uint64_t BeginWrite(DWORD safeBytes) const {
            const DWORD safe = ((safeBytes + this->bytesPerFrame - 1) / this->bytesPerFrame) % this->frames;
            const DWORD distance = (safe + this->frames - this->lastCursor) % this->frames;
            return std::max(this->tail, this->play + distance + this->safetyFrames);
        }

        /* Leave time for the cursor to advance while the buffer is locked. */
        uint64_t EndWrite() const { return this->play + this->frames - this->safetyFrames; }

        HRESULT Write(IDirectSoundBuffer* buffer, uint64_t start, DWORD count, const float* samples) {
            if (count) {
                /* Reserve bookkeeping before touching the device buffer. */
                this->segments.push_back({ start, start + count });
            }
            const HRESULT result = this->Fill(buffer, start, count, samples);
            if (FAILED(result) && count) {
                this->segments.pop_back();
            }
            else if (count) {
                this->tail = start + count;
            }
            return result;
        }

        HRESULT Silence(IDirectSoundBuffer* buffer, uint64_t start, DWORD count) {
            return this->Fill(buffer, start, count, nullptr);
        }

    private:
        struct Segment {
            uint64_t begin;
            uint64_t end;
        };

        HRESULT Fill(IDirectSoundBuffer* buffer, uint64_t start, DWORD count, const float* samples) {
            if (!count) {
                return DS_OK;
            }
            void* first = nullptr;
            void* second = nullptr;
            DWORD firstBytes = 0;
            DWORD secondBytes = 0;
            const DWORD offset = static_cast<DWORD>(start % this->frames) * this->bytesPerFrame;
            const HRESULT result =
                buffer->Lock(offset, count * this->bytesPerFrame, &first, &firstBytes, &second, &secondBytes, 0);
            if (FAILED(result)) {
                return result;
            }
            if (samples) {
                std::memcpy(first, samples, firstBytes);
                if (secondBytes) {
                    std::memcpy(second, reinterpret_cast<const char*>(samples) + firstBytes, secondBytes);
                }
            }
            else {
                std::memset(first, 0, firstBytes);
                if (secondBytes) {
                    std::memset(second, 0, secondBytes);
                }
            }
            return buffer->Unlock(first, firstBytes, second, secondBytes);
        }

        DWORD frames = 0;
        DWORD bytesPerFrame = 0;
        DWORD safetyFrames = 0;
        DWORD lastCursor = 0;
        uint64_t play = 0;
        uint64_t tail = 0;
        std::deque<Segment> segments;
    };
}

/* The worker owns playback progress and refills the software DirectSound ring.
The host queue is bounded independently of that ring. Audio is copied into the
queue before its token is returned; Stop joins the worker and finishes callbacks.
The worker checks the play and safe-write cursors at a short interval: DirectSound
notifications are not reliable on every driver. Unused writable space is filled
with silence so a normal underrun does not replay old samples. */
struct DirectSoundOutput::State {
    using Clock = std::chrono::steady_clock;
    struct QueuedPcm {
        std::vector<float> samples;
        size_t offset = 0;
    };

    /* A single atomic snapshot keeps concurrent GetTiming calls bounded. */
    struct alignas(16) Snapshot {
        alignas(16) volatile LONG64 words[2] {};
        void Publish(uint64_t generation, uint64_t frames, uint64_t timestamp) noexcept {
            constexpr uint64_t mask = (uint64_t { 1 } << 48) - 1;
            const uint64_t packed = generation && frames <= mask ? (generation << 48) | frames : 0;
            alignas(16) LONG64 expected[2] {};
            while (!_InterlockedCompareExchange128(
                words, static_cast<LONG64>(packed ? timestamp : 0), static_cast<LONG64>(packed), expected))
            {
            }
        }
        void Read(mc_output_timing& timing) const noexcept {
            alignas(16) LONG64 result[2] {};
            _InterlockedCompareExchange128(const_cast<volatile LONG64*>(words), 0, 0, result);
            const auto packed = static_cast<uint64_t>(result[0]);
            if (packed >> 48) {
                timing.valid_flags = MC_OUTPUT_TIMING_PLAYED | MC_OUTPUT_TIMING_ESTIMATED;
                timing.generation = packed >> 48;
                timing.frames_played_by_driver = packed & ((uint64_t { 1 } << 48) - 1);
                timing.frames_played_timestamp_ns = static_cast<uint64_t>(result[1]);
            }
        }
    } timing;

    std::mutex mutex;
    std::condition_variable changed;
    std::thread worker;
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    struct HostState {
        mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
        /* Copied PCM awaiting transfer; both blocks and frames are bounded. */
        musik::FixedRingQueue<QueuedPcm, 256> queue;
        size_t capacity = 0;
        /* Frames waiting in the host queue or the DirectSound buffer. */
        size_t outstanding = 0;
    } host;
    struct DirectSoundState {
        ComPtr<IDirectSound8> device;
        ComPtr<IDirectSoundBuffer> buffer;
        /* Tracks device buffer positions and the audio copied there. */
        DeviceRing ring;
        DWORD ringFrames = 0;
        DWORD frameBytes = 0;
        Clock::time_point lastUpdate;
    } directSound;
    uint64_t played = 0;
    uint64_t generation = 0;
    double volume = 1;
    bool stopped = true;
    bool paused = false;
    mc_result failure = MC_OK;

    void Publish() noexcept {
        const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
        if (now > 0) {
            timing.Publish(generation, played, static_cast<uint64_t>(now));
        }
    }

    bool Tick() {
        DWORD playBytes = 0, safeBytes = 0;
        if (FAILED(directSound.buffer->GetCurrentPosition(&playBytes, &safeBytes))) {
            return false;
        }
        const auto now = Clock::now();
        /* A full unnoticed wrap makes cursor accounting ambiguous. Fail the
        stream instead of pretending old PCM was played successfully. */
        if (now - directSound.lastUpdate >=
            std::chrono::duration<double>(double(directSound.ringFrames) / format.sample_rate))
        {
            return false;
        }
        directSound.lastUpdate = now;
        /* Translate the play cursor to an absolute position and retire only
        the audio segments it crossed. Silence is excluded from played timing. */
        const size_t consumed = directSound.ring.Advance(playBytes);
        host.outstanding -= consumed;
        played += consumed;
        if (consumed) {
            changed.notify_all();
            if (host.callbacks.ready) {
                host.callbacks.ready(host.callbacks.context);
            }
        }
        Publish();

        /* The region between the play and safe-write cursors is controlled by
        DirectSound. Append audio only beyond it and beyond previously queued
        PCM, leaving the rest of the writable region silent. */
        uint64_t position = directSound.ring.BeginWrite(safeBytes);
        const uint64_t limit = directSound.ring.EndWrite();
        while (position < limit && !host.queue.Empty()) {
            /* Do not start another write using a stale cursor sample. */
            if (Clock::now() - now >= std::chrono::milliseconds(15)) {
                return false;
            }
            auto& pcm = *host.queue.Front();
            const size_t remaining = pcm.samples.size() / format.channels - pcm.offset;
            const auto count = static_cast<DWORD>(std::min<uint64_t>(remaining, limit - position));
            if (FAILED(directSound.ring.Write(
                    directSound.buffer.Get(), position, count, pcm.samples.data() + pcm.offset * format.channels)))
            {
                return false;
            }
            position += count;
            pcm.offset += count;
            if (pcm.offset == pcm.samples.size() / format.channels) {
                host.queue.TryPop();
                if (host.callbacks.ready) {
                    host.callbacks.ready(host.callbacks.context);
                }
            }
        }
        /* Clear the remainder of the writable region, including old audio. */
        if (Clock::now() - now >= std::chrono::milliseconds(15)) {
            return false;
        }
        return position >= limit ||
            SUCCEEDED(
                directSound.ring.Silence(directSound.buffer.Get(), position, static_cast<DWORD>(limit - position)));
    }

    void Run() noexcept {
        std::unique_lock lock(mutex);
        while (!stopped) {
            if (!paused) {
                try {
                    if (!Tick()) {
                        failure = MC_ERROR_IO;
                    }
                }
                catch (...) {
                    failure = MC_ERROR_INTERNAL;
                }
                if (failure != MC_OK) {
                    /* A lost or ambiguous cursor must not leave stale audio
                    looping while the host reacts to the reported failure. */
                    directSound.buffer->Stop();
                    changed.notify_all();
                    if (host.callbacks.ready) {
                        host.callbacks.ready(host.callbacks.context);
                    }
                    break;
                }
            }
            /* DirectSound cursor notifications can be unreliable on some
            drivers. Recheck the cursor periodically; queue, pause, and stop
            also wake this condition variable without waiting for the timeout.
            Microsoft discusses notification reliability and polling frequency:
            https://learn.microsoft.com/en-us/windows/win32/dxtecharts/directx-9-frequently-asked-questions */
            changed.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
};

DirectSoundOutput::DirectSoundOutput()
: state(std::make_unique<State>()) {
}

DirectSoundOutput::~DirectSoundOutput() noexcept {
    Stop();
}

mc_result DirectSoundOutput::Prepare(const mc_output_options& options, const mc_output_callbacks& callbacks) {
    auto& s = *state;
    if (s.worker.joinable()) {
        return MC_ERROR_IN_FLIGHT;
    }

    /* Check the sizes before narrowing them to the fields in WAVEFORMATEX. */
    if (!options.format.channels || options.format.channels > USHRT_MAX / sizeof(float) ||
        options.format.sample_rate > UINT_MAX / (options.format.channels * sizeof(float)))
    {
        return MC_ERROR_UNSUPPORTED;
    }

    /* An empty device ID selects the system default. Otherwise, convert the
    host's UTF-8 DirectSound GUID into the form required by DirectSoundCreate8. */
    GUID id {};
    GUID* selected = nullptr;
    if (options.device_id.length) {
        if (options.device_id.length > INT_MAX) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, options.device_id.data,
            static_cast<int>(options.device_id.length), nullptr, 0);
        if (!length) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        std::wstring text(static_cast<size_t>(length), L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, options.device_id.data,
                static_cast<int>(options.device_id.length), text.data(), length) ||
            FAILED(CLSIDFromString(text.c_str(), &id)))
        {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        selected = &id;
    }

    /* The desktop window supplies the cooperative-level owner; the secondary
    buffer below contains the stream that DirectSound mixes for playback. */
    if (FAILED(DirectSoundCreate8(selected, s.directSound.device.GetAddressOf(), nullptr)) ||
        FAILED(s.directSound.device->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY)))
    {
        return MC_ERROR_IO;
    }

    /* The host supplies interleaved 32-bit floating-point PCM. */
    WAVEFORMATEXTENSIBLE wave {};
    wave.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wave.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wave.Format.nChannels = static_cast<WORD>(options.format.channels);
    wave.Format.nSamplesPerSec = options.format.sample_rate;
    wave.Format.wBitsPerSample = 32;
    wave.Format.nBlockAlign = static_cast<WORD>(options.format.channels * sizeof(float));
    wave.Format.nAvgBytesPerSec = wave.Format.nSamplesPerSec * wave.Format.nBlockAlign;
    wave.Samples.wValidBitsPerSample = 32;
    wave.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

    /* Supply speaker masks for the supported common channel layouts. */
    switch (options.format.channels) {
        case 1:
            wave.dwChannelMask = KSAUDIO_SPEAKER_MONO;
            break;
        case 2:
            wave.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
            break;
        case 4:
            wave.dwChannelMask = KSAUDIO_SPEAKER_QUAD;
            break;
        case 6:
            wave.dwChannelMask = KSAUDIO_SPEAKER_5POINT1;
            break;
    }

    s.format = options.format;
    s.directSound.frameBytes = wave.Format.nBlockAlign;

    /* Give the looping device buffer roughly half a second of capacity. Its
    size is separate from the host queue's limit below. */
    s.directSound.ringFrames = std::max<DWORD>(options.format.sample_rate / 2, 256);
    if (s.directSound.ringFrames > DSBSIZE_MAX / s.directSound.frameBytes) {
        return MC_ERROR_UNSUPPORTED;
    }

    /* Use a software buffer and request the more accurate play cursor. */
    DSBUFFERDESC description {};
    description.dwSize = sizeof(description);
    description.dwFlags = DSBCAPS_LOCSOFTWARE | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS;
    description.dwBufferBytes = s.directSound.ringFrames * s.directSound.frameBytes;
    description.lpwfxFormat = &wave.Format;
    if (FAILED(s.directSound.device->CreateSoundBuffer(&description, s.directSound.buffer.GetAddressOf(), nullptr))) {
        return MC_ERROR_IO;
    }

    /* Start a new queue and timing generation for this preparation. */
    s.host.capacity = options.queue_capacity_frames ? options.queue_capacity_frames : 32768;
    s.host.callbacks = callbacks;
    s.host.outstanding = 0;
    s.played = 0;
    /* Reserve 25 ms on both edges of the writable window for cursor movement
    and scheduling jitter, bounded so small buffers remain usable. */
    const DWORD safetyFrames = std::min<DWORD>(s.directSound.ringFrames / 4,
        std::max<DWORD>(1, options.format.sample_rate / 40 + (options.format.sample_rate % 40 != 0)));
    s.directSound.ring.Reset(s.directSound.ringFrames, s.directSound.frameBytes, safetyFrames);
    while (s.host.queue.TryPop()) {
    }
    s.failure = MC_OK;
    s.paused = false;
    s.generation = s.generation == UINT16_MAX ? 1 : s.generation + 1;

    /* DirectSound buffers have unspecified initial contents. Clear the entire
    ring before starting so unfilled regions cannot play uninitialized audio. */
    if (FAILED(s.directSound.ring.Silence(s.directSound.buffer.Get(), 0, s.directSound.ringFrames)) ||
        FAILED(s.directSound.buffer->SetCurrentPosition(0)))
    {
        return MC_ERROR_IO;
    }

    /* Apply the host volume before starting the silent looping buffer. */
    s.volume = options.initial_volume;
    const LONG db = options.initial_volume <= 0
        ? DSBVOLUME_MIN
        : static_cast<LONG>(std::clamp(2000.0 * std::log10(options.initial_volume), double(DSBVOLUME_MIN), 0.0));
    if (FAILED(s.directSound.buffer->SetVolume(db)) || FAILED(s.directSound.buffer->Play(0, 0, DSBPLAY_LOOPING))) {
        return MC_ERROR_IO;
    }

    /* Publish the initial snapshot and let the worker handle subsequent cursor
    updates and transfers from the host queue. */
    s.stopped = false;
    s.directSound.lastUpdate = State::Clock::now();
    s.Publish();
    s.worker = std::thread([&s] {
        s.Run();
    });
    return MC_OK;
}

mc_result DirectSoundOutput::QueueAudioBuffer(const mc_output_buffer& buffer) noexcept {
    auto& s = *state;
    std::lock_guard lock(s.mutex);
    if (s.stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (s.failure != MC_OK) {
        return s.failure;
    }
    if (buffer.pcm.format.sample_rate != s.format.sample_rate || buffer.pcm.format.channels != s.format.channels ||
        buffer.pcm.format.channel_layout != s.format.channel_layout)
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer.pcm.frame_count > s.host.capacity) {
        return MC_ERROR_BUFFER_TOO_SMALL;
    }
    if (buffer.pcm.frame_count > s.host.capacity - s.host.outstanding) {
        return MC_ERROR_QUEUE_FULL;
    }
    try {
        State::QueuedPcm pcm;
        pcm.samples.assign(buffer.pcm.data, buffer.pcm.data + buffer.pcm.frame_count * s.format.channels);
        if (!s.host.queue.TryPush(std::move(pcm))) {
            return MC_ERROR_QUEUE_FULL;
        }
    }
    catch (...) {
        return MC_ERROR_INTERNAL;
    }
    s.host.outstanding += buffer.pcm.frame_count;
    s.host.callbacks.completed(s.host.callbacks.context, buffer.token, MC_OK);
    s.changed.notify_all();
    return MC_OK;
}

mc_result DirectSoundOutput::Pause() {
    auto& s = *state;
    std::lock_guard lock(s.mutex);
    if (s.stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (s.failure != MC_OK) {
        return s.failure;
    }
    if (s.paused) {
        return MC_OK;
    }
    if (!s.Tick() || FAILED(s.directSound.buffer->Stop())) {
        return MC_ERROR_IO;
    }
    s.paused = true;
    s.changed.notify_all();
    return MC_OK;
}

mc_result DirectSoundOutput::Resume() {
    auto& s = *state;
    std::lock_guard lock(s.mutex);
    if (s.stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (s.failure != MC_OK) {
        return s.failure;
    }
    if (!s.paused) {
        return MC_OK;
    }
    if (FAILED(s.directSound.buffer->Play(0, 0, DSBPLAY_LOOPING))) {
        return MC_ERROR_IO;
    }
    s.directSound.lastUpdate = State::Clock::now();
    s.paused = false;
    s.changed.notify_all();
    if (s.host.callbacks.ready) {
        s.host.callbacks.ready(s.host.callbacks.context);
    }
    return MC_OK;
}

mc_result DirectSoundOutput::SetVolume(double value) {
    auto& s = *state;
    std::lock_guard lock(s.mutex);
    s.volume = value;
    const LONG db = value <= 0 ? DSBVOLUME_MIN
                               : static_cast<LONG>(std::clamp(2000.0 * std::log10(value), double(DSBVOLUME_MIN), 0.0));
    return !s.directSound.buffer || SUCCEEDED(s.directSound.buffer->SetVolume(db)) ? MC_OK : MC_ERROR_IO;
}

double DirectSoundOutput::GetVolume() const {
    std::lock_guard lock(state->mutex);
    return state->volume;
}

mc_result DirectSoundOutput::Stop() noexcept {
    auto& s = *state;
    {
        std::lock_guard lock(s.mutex);
        s.stopped = true;
        s.changed.notify_all();
    }
    if (s.worker.joinable()) {
        s.worker.join();
    }
    if (s.directSound.buffer) {
        s.directSound.buffer->Stop();
    }
    {
        std::lock_guard lock(s.mutex);
        s.directSound.buffer.Reset();
        s.directSound.device.Reset();
        while (s.host.queue.TryPop()) {
        }
        s.host.outstanding = 0;
    }
    s.timing.Publish(0, 0, 0);
    return MC_OK;
}

mc_result DirectSoundOutput::Drain() {
    auto& s = *state;
    std::unique_lock lock(s.mutex);
    if (s.stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (s.paused && s.host.outstanding) {
        return MC_ERROR_IN_FLIGHT;
    }
    s.changed.wait(lock, [&] {
        return s.stopped || s.failure != MC_OK || !s.host.outstanding;
    });
    return s.stopped ? MC_ERROR_CANCELLED : s.failure;
}

mc_result DirectSoundOutput::GetTiming(mc_output_timing& timing) const noexcept {
    state->timing.Read(timing);
    return MC_OK;
}

mc_result DirectSoundOutput::VisitDevices(mc_output_device_fn read, void* user) {
    return directsound::VisitDevices(read, user);
}

mc_result DirectSoundOutput::ReadDefaultDevice(mc_output_device_fn read, void* user) {
    return directsound::ReadDefaultDevice(read, user);
}

mc_result DirectSoundOutput::GetDefaultFormat(mc_string_view, mc_audio_format&) {
    return MC_ERROR_UNSUPPORTED;
}
