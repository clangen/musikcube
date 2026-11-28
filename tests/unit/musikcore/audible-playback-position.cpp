#include <musikcore/audio/AudiblePlaybackPosition.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using musik::core::audio::AudiblePlaybackPosition;

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::fprintf(stderr, "audible playback position test failed at line %d: %s\n", __LINE__, #condition);              \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while (false)

namespace {
    mc_pcm_view audio(size_t frames, uint32_t rate = 48000) {
        static std::array<float, 96000> samples {};
        CHECK(frames <= samples.size() / 2);
        auto pcm = mc_pcm_view MC_PCM_VIEW_DEFAULT;
        pcm.data = samples.data();
        pcm.frame_count = frames;
        pcm.format.sample_rate = rate;
        pcm.format.channels = 2;
        return pcm;
    }

    mc_output_timing played(uint64_t frames, uint64_t generation = 1) {
        auto timing = mc_output_timing MC_OUTPUT_TIMING_DEFAULT;
        timing.valid_flags = MC_OUTPUT_TIMING_PLAYED;
        timing.generation = generation;
        timing.frames_played_by_driver = frames;
        timing.frames_played_timestamp_ns = 1;
        return timing;
    }

    bool near(std::optional<double> position, double expected) {
        return position && std::abs(*position - expected) < 1e-9;
    }

    void checkKnownTiming() {
        AudiblePlaybackPosition progress;
        AudiblePlaybackPosition::TrackPosition track;
        CHECK(!progress.UpdateAudibleFrame(MC_OK, played(12000)));
        CHECK(!progress.GetTrackSeconds(track, 12000));
        const auto accepted = progress.RecordQueuedBuffer(track, audio(48000), 50.0);
        auto timing = played(24000);
        timing.valid_flags |= MC_OUTPUT_TIMING_QUEUED | MC_OUTPUT_TIMING_DEVICE_LATENCY;
        timing.queued_seconds = 0.5;
        timing.device_latency_seconds = 0.1;
        const auto cursor = progress.UpdateAudibleFrame(MC_OK, timing);
        CHECK(cursor == 19200);
        CHECK(near(progress.GetTrackSeconds(track, cursor), 50.4));
        CHECK(near(progress.GetTrackSeconds(track, cursor), 50.4));

        /* returning storage does not supersede a measured audible cursor. */
        progress.RecordReturnedBuffer(accepted);
        CHECK(progress.UpdateAudibleFrame(MC_OK, timing) == 19200);
        CHECK(!progress.UpdateAudibleFrame(MC_ERROR_BUSY, timing));
        CHECK(near(progress.GetTrackSeconds(track, std::nullopt), 50.4));
    }

    void checkFallbacks() {
        {
            AudiblePlaybackPosition progress;
            AudiblePlaybackPosition::TrackPosition track;
            const auto first = progress.RecordQueuedBuffer(track, audio(12000), 0.0);
            progress.RecordQueuedBuffer(track, audio(12000), 0.25);
            progress.RecordReturnedBuffer(first);
            auto unknown = mc_output_timing MC_OUTPUT_TIMING_DEFAULT;
            unknown.queued_seconds = std::numeric_limits<double>::quiet_NaN();
            unknown.device_latency_seconds = std::numeric_limits<double>::quiet_NaN();
            CHECK(progress.UpdateAudibleFrame(MC_OK, unknown) == 12000);
            CHECK(near(progress.GetTrackSeconds(track, progress.UpdateAudibleFrame(MC_ERROR_IO, played(24000))), 0.25));
        }
        {
            AudiblePlaybackPosition progress;
            AudiblePlaybackPosition::TrackPosition track;
            progress.RecordQueuedBuffer(track, audio(48000), 0.0);
            auto queued = mc_output_timing MC_OUTPUT_TIMING_DEFAULT;
            queued.valid_flags = MC_OUTPUT_TIMING_QUEUED | MC_OUTPUT_TIMING_DEVICE_LATENCY;
            queued.queued_seconds = 0.25;
            queued.device_latency_seconds = 0.125;
            CHECK(progress.UpdateAudibleFrame(MC_OK, queued) == 30000);
            CHECK(near(progress.GetTrackSeconds(track, progress.UpdateAudibleFrame(MC_OK, queued)), 0.625));
            queued.queued_seconds = 0.5;
            CHECK(progress.UpdateAudibleFrame(MC_OK, queued) == 30000);
        }
    }

    void checkNativeRestartAndSeek() {
        AudiblePlaybackPosition progress;
        AudiblePlaybackPosition::TrackPosition track;
        const auto first = progress.RecordQueuedBuffer(track, audio(12000), 0.0);
        const auto last = progress.RecordQueuedBuffer(track, audio(36000), 0.25);
        progress.RecordReturnedBuffer(first);
        CHECK(progress.UpdateAudibleFrame(MC_OK, played(12000, 7)) == 12000);
        CHECK(progress.UpdateAudibleFrame(MC_OK, played(0, 8)) == 12000);
        CHECK(progress.UpdateAudibleFrame(MC_OK, played(48000, 8)) == 12000);
        auto queued = played(0, 8);
        queued.valid_flags |= MC_OUTPUT_TIMING_QUEUED;
        queued.queued_seconds = 0.25;
        CHECK(progress.UpdateAudibleFrame(MC_OK, queued) == 36000);
        CHECK(near(progress.GetTrackSeconds(track, 36000), 0.75));

        progress.Reset();
        CHECK(!progress.GetTrackSeconds(track, 36000));
        CHECK(!progress.UpdateAudibleFrame(MC_OK, played(36000, 8)));
        const auto fresh = progress.RecordQueuedBuffer(track, audio(44100, 44100), 3.0);
        CHECK(fresh.FirstFrame() == 0);
        CHECK(progress.UpdateAudibleFrame(MC_OK, played(22050, 9)) == 22050);
        CHECK(near(progress.GetTrackSeconds(track, 22050), 3.5));
        progress.RecordReturnedBuffer(last);
        CHECK(progress.UpdateAudibleFrame(MC_OK, mc_output_timing MC_OUTPUT_TIMING_DEFAULT) == 22050);
    }

    void checkGaplessTrackPositions() {
        AudiblePlaybackPosition progress;
        AudiblePlaybackPosition::TrackPosition outgoing;
        AudiblePlaybackPosition::TrackPosition incoming;
        const auto first = progress.RecordQueuedBuffer(outgoing, audio(48000), 10.0);
        const auto second = progress.RecordQueuedBuffer(incoming, audio(48000), 0.0);
        CHECK(first.FirstFrame() == 0 && second.FirstFrame() == 48000);
        auto cursor = progress.UpdateAudibleFrame(MC_OK, played(24000));
        CHECK(near(progress.GetTrackSeconds(outgoing, cursor), 10.5));
        CHECK(near(progress.GetTrackSeconds(incoming, cursor), 0.0));
        cursor = progress.UpdateAudibleFrame(MC_OK, played(72000));
        CHECK(near(progress.GetTrackSeconds(outgoing, cursor), 11.0));
        CHECK(near(progress.GetTrackSeconds(incoming, cursor), 0.5));
        cursor = progress.UpdateAudibleFrame(MC_OK, played(200000));
        CHECK(cursor == 96000);
        CHECK(near(progress.GetTrackSeconds(outgoing, cursor), 11.0));
        CHECK(near(progress.GetTrackSeconds(incoming, cursor), 1.0));
    }
}

int main() {
    checkKnownTiming();
    checkFallbacks();
    checkNativeRestartAndSeek();
    checkGaplessTrackPositions();
}
