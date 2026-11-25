#pragma once

#include <musikcore/audio/ITransport.h>

/* Supplies deterministic transport state without opening an audio device. */
class TestTransport final : public musik::core::audio::ITransport {
public:
    using PlaybackState = musik::core::audio::PlaybackState;
    using StreamState = musik::core::audio::StreamState;

    /* Record the URI and model prepared versus playing state without decoding or opening a device. */
    void Start(const std::string& value, Gain, StartMode mode) override {
        uri = value;
        ++starts;
        lastStartMode = mode;
        state = holdStartup
            ? PlaybackState::Stopped
            : (mode == StartMode::Wait ? PlaybackState::Prepared : PlaybackState::Playing);
        if (holdStartup) {
            stream = StreamState::Buffering;
        }
    }
    /* Ignore prefetch: these tests exercise playback commands and do not produce audio. */
    void PrepareNextTrack(const std::string&, Gain) override {}
    /* Return the last started URI so the playback service can associate its current track. */
    std::string Uri() override { return uri; }
    /* Model an immediate stopped transport; engine notifications remain under the test's control. */
    /* the fake owns no asynchronous resources; Stop behavior is tested separately. */
    void Destroy() override {}

    void Stop() override {
        state = PlaybackState::Stopped;
        stream = StreamState::Stopped;
    }
    /* Model a successful pause without asynchronous device work. */
    mc_result Pause() override { state = PlaybackState::Paused; return MC_OK; }
    /* Model a successful resume without asynchronous device work. */
    mc_result Resume() override { state = PlaybackState::Playing; return MC_OK; }
    /* Return the deterministic position set by the test or a seek command. */
    double Position() override { return position; }
    /* Record seeks in seconds so public snapshot/command units can be asserted. */
    void SetPosition(double value) override { position = value; }
    /* Expose the stored normalized volume independently of mute state. */
    double Volume() override { return volume; }
    /* Record volume commands after the public boundary validates their range. */
    void SetVolume(double value) override { volume = value; }
    /* supply a configurable duration for playback assertions. */
    double GetDuration() override { return duration; }
    /* Expose the fake output's stored mute state. */
    bool IsMuted() override { return muted; }
    /* Record mute changes without touching hardware. */
    void SetMuted(bool value) override { muted = value; }
    /* model destructive output replacement while a test holds startup pending. */
    void ReloadOutput() override {
        ++reloads;
        if (holdStartup) {
            Stop();
            uri.clear();
        }
    }
    /* Expose the transport state manipulated by commands and lifecycle cases. */
    PlaybackState GetPlaybackState() override {
        const auto result = state;
        /* model startup completing after this getter releases its snapshot lock. */
        if (completeStartupOnStateRead) {
            completeStartupOnStateRead = false;
            state = PlaybackState::Playing;
            stream = StreamState::Playing;
        }
        return result;
    }
    StreamState GetStreamState() override { return stream; }

    PlaybackState state = PlaybackState::Playing;
    StreamState stream = StreamState::Stopped;
    double duration = 30;
    double position = 0;
    double volume = 0.5;
    bool muted = false;
    unsigned reloads = 0;
    unsigned starts = 0;
    bool holdStartup = false;
    bool completeStartupOnStateRead = false;
    StartMode lastStartMode = StartMode::Immediate;
    std::string uri;
};
