#pragma once

#include <musikcore/plugin/output.hpp>
#include <memory>

class DirectSoundOutput {
public:
    DirectSoundOutput();
    ~DirectSoundOutput() noexcept;
    DirectSoundOutput(const DirectSoundOutput&) = delete;
    DirectSoundOutput& operator=(const DirectSoundOutput&) = delete;

    mc_result Prepare(const mc_output_options&, const mc_output_callbacks&);
    mc_result QueueAudioBuffer(const mc_output_buffer&) noexcept;
    mc_result Pause();
    mc_result Resume();
    mc_result SetVolume(double);
    double GetVolume() const;
    mc_result Stop() noexcept;
    mc_result Drain();
    mc_result Flush();
    mc_result GetTiming(mc_output_timing&) const noexcept;

    static mc_result VisitDevices(mc_output_device_fn, void*);
    static mc_result ReadDefaultDevice(mc_output_device_fn, void*);
    mc_result GetDefaultFormat(mc_string_view, mc_audio_format&);

private:
    struct State;
    std::unique_ptr<State> state;
};
