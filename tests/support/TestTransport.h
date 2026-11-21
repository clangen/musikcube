#pragma once

#include <musikcore/audio/ITransport.h>

/* Supplies deterministic transport state without opening an audio device. */
class TestTransport final : public musik::core::audio::ITransport {
public:
    using PlaybackState = musik::core::sdk::PlaybackState;
    using StreamState = musik::core::sdk::StreamState;

    /* Record the URI and model prepared versus playing state without decoding or opening a device. */
    void Start(const std::string& value, Gain, StartMode mode) override {
        uri = value;
        state = mode == StartMode::Wait ? PlaybackState::Prepared : PlaybackState::Playing;
    }
    /* Ignore prefetch: these tests exercise playback commands and do not produce audio. */
    void PrepareNextTrack(const std::string&, Gain) override {}
    /* Return the last started URI so the playback service can associate its current track. */
    std::string Uri() override { return uri; }
    /* Model an immediate stopped transport; engine notifications remain under the test's control. */
    void Stop() override {
        state = PlaybackState::Stopped;
        stream = StreamState::Stopped;
    }
    /* Model a successful pause without asynchronous device work. */
    bool Pause() override { state = PlaybackState::Paused; return true; }
    /* Model a successful resume without asynchronous device work. */
    bool Resume() override { state = PlaybackState::Playing; return true; }
    /* Return the deterministic position set by the test or a seek command. */
    double Position() override { return position; }
    /* Record seeks in seconds so public snapshot/command units can be asserted. */
    void SetPosition(double value) override { position = value; }
    /* Expose the stored normalized volume independently of mute state. */
    double Volume() override { return volume; }
    /* Record volume commands after the public boundary validates their range. */
    void SetVolume(double value) override { volume = value; }
    /* Supply a fixed 30-second duration for reproducible seek and snapshot assertions. */
    double GetDuration() override { return 30; }
    /* Expose the fake output's stored mute state. */
    bool IsMuted() override { return muted; }
    /* Record mute changes without touching hardware. */
    void SetMuted(bool value) override { muted = value; }
    /* Ignore device reload requests because this fixture has no physical output. */
    void ReloadOutput() override { ++reloads; }
    /* Expose the transport state manipulated by commands and lifecycle cases. */
    PlaybackState GetPlaybackState() override { return state; }
    StreamState GetStreamState() override { return stream; }

    PlaybackState state = PlaybackState::Playing;
    StreamState stream = StreamState::Stopped;
    double position = 0;
    double volume = 0.5;
    bool muted = false;
    unsigned reloads = 0;
    std::string uri;
};
