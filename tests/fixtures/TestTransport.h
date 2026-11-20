#pragma once

#include <musikcore/audio/ITransport.h>

/* Supplies deterministic transport state without opening an audio device. */
class TestTransport final : public musik::core::audio::ITransport {
public:
    using PlaybackState = musik::core::sdk::PlaybackState;
    using StreamState = musik::core::sdk::StreamState;

    void Start(const std::string&, Gain, StartMode) override { state = PlaybackState::Playing; }
    void PrepareNextTrack(const std::string&, Gain) override {}
    std::string Uri() override { return {}; }
    void Stop() override { state = PlaybackState::Stopped; }
    bool Pause() override { state = PlaybackState::Paused; return true; }
    bool Resume() override { state = PlaybackState::Playing; return true; }
    double Position() override { return position; }
    void SetPosition(double value) override { position = value; }
    double Volume() override { return volume; }
    void SetVolume(double value) override { volume = value; }
    double GetDuration() override { return 30; }
    bool IsMuted() override { return muted; }
    void SetMuted(bool value) override { muted = value; }
    void ReloadOutput() override {}
    PlaybackState GetPlaybackState() override { return state; }
    StreamState GetStreamState() override { return StreamState::Stopped; }

    PlaybackState state = PlaybackState::Playing;
    double position = 0;
    double volume = 0.5;
    bool muted = false;
};
