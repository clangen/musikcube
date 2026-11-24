/* Include the implementation so this test can exercise its module-local
device ring without exposing it as a plugin or SDK interface. */
#include <musikcore/plugin/output.hpp>
#include <memory>
#define private public
#include "../../../src/plugins/directsoundout/DirectSoundOutput.h"
#undef private
#include "../../../src/plugins/directsoundout/DirectSoundOutput.cpp"

#include <cstdio>
#include <cstdlib>

namespace {
    void Require(bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "DirectSound ring: %s\n", message);
            std::abort();
        }
    }

    /* A looping mono device with explicit cursors. It never touches the real
    audio endpoint, so this test can run while the application is playing. */
    class FakeBuffer final : public IDirectSoundBuffer {
    public:
        explicit FakeBuffer(size_t frames) : samples(frames, 0.0f) {}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
        ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
        ULONG STDMETHODCALLTYPE Release() override { return 1; }
        HRESULT STDMETHODCALLTYPE GetCaps(LPDSBCAPS) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetCurrentPosition(LPDWORD play, LPDWORD safe) override {
            if (play) *play = cursor * sizeof(float);
            if (safe) *safe = ((cursor + 10) % samples.size()) * sizeof(float);
            return DS_OK;
        }
        HRESULT STDMETHODCALLTYPE GetFormat(LPWAVEFORMATEX, DWORD, LPDWORD) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetVolume(LPLONG) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetPan(LPLONG) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetFrequency(LPDWORD) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetStatus(LPDWORD) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTSOUND, LPCDSBUFFERDESC) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE Lock(DWORD offset, DWORD bytes, LPVOID* a, LPDWORD aBytes,
            LPVOID* b, LPDWORD bBytes, DWORD) override {
            const DWORD total = static_cast<DWORD>(samples.size() * sizeof(float));
            const DWORD sampleCount = static_cast<DWORD>(samples.size());
            if (offset >= total || bytes > total) return DSERR_INVALIDPARAM;
            cursor = (cursor + advanceOnLock) % sampleCount;
            advanceOnLock = 0;
            for (DWORD i = 0; i < bytes / sizeof(float); ++i) {
                const DWORD frame = (offset / static_cast<DWORD>(sizeof(float)) + i) % sampleCount;
                const DWORD fromPlay = (frame + sampleCount - cursor) % sampleCount;
                if (fromPlay < 10) touchedUnsafeRegion = true;
            }
            *a = reinterpret_cast<char*>(samples.data()) + offset;
            *aBytes = std::min(bytes, total - offset);
            *bBytes = bytes - *aBytes;
            *b = *bBytes ? samples.data() : nullptr;
            return DS_OK;
        }
        HRESULT STDMETHODCALLTYPE Play(DWORD, DWORD, DWORD) override { return DS_OK; }
        HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetFormat(LPCWAVEFORMATEX) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetVolume(LONG) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetPan(LONG) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetFrequency(DWORD) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE Stop() override { return DS_OK; }
        HRESULT STDMETHODCALLTYPE Unlock(LPVOID, DWORD, LPVOID, DWORD) override { return DS_OK; }
        HRESULT STDMETHODCALLTYPE Restore() override { return E_NOTIMPL; }

        std::vector<float> samples;
        DWORD cursor = 0;
        DWORD advanceOnLock = 0;
        bool touchedUnsafeRegion = false;
    };
}

int main() {
    constexpr DWORD ringFrames = 256;
    FakeBuffer device(ringFrames);
    DirectSoundOutput::State state;
    state.format = { sizeof(mc_audio_format), 1000, 1, 0 };
    state.directSound.buffer.Attach(&device);
    state.directSound.ringFrames = ringFrames;
    state.directSound.frameBytes = sizeof(float);
    state.directSound.ring.Reset(ringFrames, sizeof(float), 25);
    state.directSound.lastUpdate = DirectSoundOutput::State::Clock::now();
    state.generation = 1;

    DirectSoundOutput::State::QueuedPcm first;
    first.samples.assign(512, 0.25f);
    DirectSoundOutput::State::QueuedPcm second;
    second.samples.assign(512, 0.75f);
    Require(state.host.queue.TryPush(std::move(first)) && state.host.queue.TryPush(std::move(second)), "queue tracks");
    state.host.outstanding = 1024;

    std::vector<float> heard;
    auto advance = [&](size_t frames, size_t serviceInterval = 5) {
        for (size_t i = 0; i < frames; ++i) {
            heard.push_back(device.samples[device.cursor]);
            device.cursor = (device.cursor + 1) % ringFrames;
            if (i % serviceInterval == serviceInterval - 1) Require(state.Tick(), "service simulated cursor");
        }
    };
    Require(state.Tick(), "prime device");
    advance(1400);
    const auto start = std::find(heard.begin(), heard.end(), 0.25f);
    Require(start != heard.end(), "first track reaches device");
    Require(std::distance(start, heard.end()) >= 1024, "both tracks reach device");
    Require(std::all_of(start, start + 512, [](float sample) { return sample == 0.25f; }),
        "first track survives multiple cursor wraps");
    Require(std::all_of(start + 512, start + 1024, [](float sample) { return sample == 0.75f; }),
        "second track follows without a gap or repeated samples");
    Require(std::all_of(start + 1024, heard.end(), [](float sample) { return sample == 0.0f; }),
        "end of track drains to silence without replay");
    Require(state.host.outstanding == 0 && state.played == 1024, "timing excludes silence");

    /* Simulate a producer underrun longer than one ring, then another track. */
    heard.clear();
    advance(512);
    Require(std::all_of(heard.begin(), heard.end(), [](float sample) { return sample == 0.0f; }),
        "an idle producer does not replay audio");
    DirectSoundOutput::State::QueuedPcm later;
    later.samples.assign(80, 0.5f);
    Require(state.host.queue.TryPush(std::move(later)), "queue after underrun");
    state.host.outstanding = 80;
    heard.clear();
    Require(state.Tick(), "service resumed producer");
    advance(400);
    const auto resumed = std::find(heard.begin(), heard.end(), 0.5f);
    Require(resumed != heard.end() && std::distance(resumed, heard.end()) >= 80, "audio resumes after underrun");
    Require(std::all_of(resumed, resumed + 80, [](float sample) { return sample == 0.5f; }),
        "resumed samples are contiguous");
    Require(std::all_of(resumed + 80, heard.end(), [](float sample) { return sample == 0.0f; }),
        "resumed track drains to silence");

    /* Simulate a worker that runs less frequently, as under CPU load, while
    still servicing the device within the ring's safety margin. */
    device.cursor = 0;
    std::fill(device.samples.begin(), device.samples.end(), 0.0f);
    state.directSound.ring.Reset(ringFrames, sizeof(float), 25);
    state.directSound.lastUpdate = DirectSoundOutput::State::Clock::now();
    state.host.outstanding = 700;
    DirectSoundOutput::State::QueuedPcm loaded;
    loaded.samples.assign(700, 0.375f);
    Require(state.host.queue.TryPush(std::move(loaded)), "queue for delayed servicing");
    heard.clear();
    Require(state.Tick(), "prime delayed servicing");
    advance(1100, 17);
    const auto loadedStart = std::find(heard.begin(), heard.end(), 0.375f);
    Require(loadedStart != heard.end() && std::distance(loadedStart, heard.end()) >= 700,
        "delayed worker plays all samples");
    Require(std::all_of(loadedStart, loadedStart + 700, [](float sample) { return sample == 0.375f; }),
        "delayed servicing preserves samples across wraps");
    Require(std::all_of(loadedStart + 700, heard.end(), [](float sample) { return sample == 0.0f; }),
        "delayed servicing drains to silence");

    /* A missed full cycle must be reported; cursor arithmetic cannot tell how
    many times the device wrapped while the worker was suspended. */
    state.directSound.lastUpdate -= std::chrono::milliseconds(300);
    Require(!state.Tick(), "detect an ambiguous full-cycle worker stall");
    state.directSound.buffer.Detach();

    DeviceRing guarded;
    FakeBuffer advancingDevice(ringFrames);
    guarded.Reset(ringFrames, sizeof(float), 25);
    guarded.Advance(0);
    const auto begin = guarded.BeginWrite(10 * sizeof(float));
    const auto end = guarded.EndWrite();
    std::vector<float> window(static_cast<size_t>(end - begin), 0.25f);
    advancingDevice.advanceOnLock = 20;
    Require(SUCCEEDED(guarded.Write(&advancingDevice, begin, static_cast<DWORD>(window.size()), window.data())),
        "write while the play cursor advances");
    Require(!advancingDevice.touchedUnsafeRegion, "safety margin covers cursor movement during lock");
}
