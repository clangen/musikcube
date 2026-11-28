#include "AudiblePlaybackPosition.h"
#include <common/Contract.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace musik { namespace core { namespace audio {

    AudiblePlaybackPosition::BufferPosition AudiblePlaybackPosition::RecordQueuedBuffer(
        TrackPosition& track, const mc_pcm_view& pcm, double mediaStart) noexcept
    {
        musik::contract::Assert(!this->sampleRate || this->sampleRate == pcm.format.sample_rate,
            "output sample rate changed without resetting playback progress");
        musik::contract::Assert(pcm.frame_count <= std::numeric_limits<uint64_t>::max() - this->submittedFrames,
            "output frame identity exhausted");
        this->sampleRate = pcm.format.sample_rate;
        const auto firstFrame = this->submittedFrames;
        this->submittedFrames += pcm.frame_count;
        if (track.resetId != this->resetId) {
            track.resetId = this->resetId;
            track.firstFrame = firstFrame;
            track.mediaStart = mediaStart;
            track.lastKnown = mediaStart;
        }
        track.endFrame = this->submittedFrames;
        return BufferPosition(this->resetId, firstFrame, this->submittedFrames);
    }

    void AudiblePlaybackPosition::RecordReturnedBuffer(const BufferPosition& bufferPosition) noexcept {
        if (bufferPosition.resetId == this->resetId) {
            this->completedFrames = std::max(this->completedFrames, bufferPosition.endFrame);
        }
    }

    void AudiblePlaybackPosition::Reset() noexcept {
        musik::contract::Assert(
            this->resetId != std::numeric_limits<uint64_t>::max(), "playback progress reset identity exhausted");
        ++this->resetId;
        this->submittedFrames = 0;
        this->completedFrames = 0;
        this->lastAudibleFrame = 0;
        this->sampleRate = 0;
        this->nativeResetId.reset();
        this->nativeCounterRestarted = false;
    }

    std::optional<uint64_t> AudiblePlaybackPosition::UpdateAudibleFrame(
        mc_result status, const mc_output_timing& raw) noexcept {
        if (status == MC_ERROR_BUSY || !this->sampleRate) {
            return std::nullopt;
        }
        const auto timing = status == MC_OK ? raw : mc_output_timing MC_OUTPUT_TIMING_DEFAULT;
        if (timing.valid_flags & MC_OUTPUT_TIMING_PLAYED) {
            if (this->nativeResetId && *this->nativeResetId != timing.generation) {
                /* the plugin restarted its frame counter without an Output
                seek/stop. its new zero cannot describe audio already queued. */
                this->nativeCounterRestarted = true;
            }
            this->nativeResetId = timing.generation;
        }
        uint64_t audible = this->completedFrames;
        if ((timing.valid_flags & MC_OUTPUT_TIMING_PLAYED) && !this->nativeCounterRestarted) {
            audible = timing.frames_played_by_driver;
        }
        else if ((timing.valid_flags & MC_OUTPUT_TIMING_QUEUED) && std::isfinite(timing.queued_seconds) &&
            timing.queued_seconds >= 0)
        {
            const double queued = std::ceil(timing.queued_seconds * this->sampleRate);
            audible = queued < static_cast<double>(this->submittedFrames)
                ? this->submittedFrames - static_cast<uint64_t>(queued)
                : 0;
        }
        if ((timing.valid_flags & MC_OUTPUT_TIMING_DEVICE_LATENCY) && std::isfinite(timing.device_latency_seconds) &&
            timing.device_latency_seconds > 0)
        {
            const double delay = std::ceil(timing.device_latency_seconds * this->sampleRate);
            audible = delay < static_cast<double>(audible) ? audible - static_cast<uint64_t>(delay) : 0;
        }
        /* uncertain estimates can become more conservative, but sound already
        heard cannot become unheard since the last output reset. */
        this->lastAudibleFrame = std::clamp(audible, this->lastAudibleFrame, this->submittedFrames);
        return this->lastAudibleFrame;
    }

    std::optional<double> AudiblePlaybackPosition::GetTrackSeconds(
        TrackPosition& track, std::optional<uint64_t> audibleFrame) const noexcept
    {
        if (track.resetId != this->resetId || !this->sampleRate) {
            return std::nullopt;
        }
        if (audibleFrame) {
            const auto frames = std::clamp(*audibleFrame, track.firstFrame, track.endFrame) - track.firstFrame;
            track.lastKnown = track.mediaStart + static_cast<double>(frames) / this->sampleRate;
        }
        return track.lastKnown;
    }

} } }
