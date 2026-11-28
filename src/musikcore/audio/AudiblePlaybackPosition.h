#pragma once

#include <common/DeleteDefaults.h>
#include <musikcore/plugin/output.h>
#include <optional>

namespace musik { namespace core { namespace audio {

    /* tracks the playback position the listener is currently hearing at the
    speakers. decoded or queued audio can be further ahead because it is still
    waiting to play.

    one PCM frame contains one sample for every channel. at 48 kHz, if a track
    starts at output frame 48000 and the output reaches frame 72000, that track
    is 0.5 seconds in. gapless tracks share the output's running frame count,
    but each track has its own starting media seconds and audible end.

    Output records which frames it queued and which buffers returned, then
    supplies the device's timing reading. this class calculates one audible
    frame count for both track position and visualization, accounting for device
    delay once. it stores positions and counts, never audio samples.

    Output owns this object. its lifecycle mutex guards every call, including
    access to each Writer's TrackPosition. no device, thread, or mutex is owned
    here. */
    class AudiblePlaybackPosition {
    public:
        /* small metadata value kept beside an output buffer, not the buffer
        or its audio storage. it records where that buffer's frames were queued
        and which output reset they belong to. returned buffers keep this value
        so old audio recycled after a seek cannot advance the new playback. */
        class BufferPosition {
        public:
            BufferPosition() = default;

            /* VisualizationAudioQueue uses this zero-based output frame index
            to schedule its separate PCM copy at the same playback position. */
            uint64_t FirstFrame() const noexcept { return this->firstFrame; }

        private:
            friend class AudiblePlaybackPosition;

            BufferPosition(uint64_t resetId, uint64_t firstFrame, uint64_t endFrame) noexcept
            : resetId(resetId)
            , firstFrame(firstFrame)
            , endFrame(endFrame) {}

            uint64_t resetId = 0;
            uint64_t firstFrame = 0;
            uint64_t endFrame = 0;
        };

        /* remembers where one track begins in the output's frame count, so
        an audible output frame count can become seconds within that track.
        each Writer owns one value; the output clock is shared separately.
        the remembered end caps an outgoing track before its successor, and
        lastKnown keeps the previous seconds while a device reading is busy.
        this value owns no audio samples, track object, or clock. */
        class TrackPosition {
        public:
            MUSIK_DELETE_COPY_AND_MOVE(TrackPosition);

            TrackPosition() = default;

        private:
            friend class AudiblePlaybackPosition;

            uint64_t resetId = 0;
            uint64_t firstFrame = 0;
            uint64_t endFrame = 0;
            double mediaStart = 0.0;
            double lastKnown = 0.0;
        };

        MUSIK_DELETE_COPY_AND_MOVE(AudiblePlaybackPosition);

        AudiblePlaybackPosition() = default;

        /* Output's producer calls this after the plugin accepts a PCM buffer.
        pcm has already been validated by Output; mediaStart is the media time
        of the buffer's first frame, in seconds. remember this track's first
        queued frame and extend its end, then return metadata for that buffer's
        eventual storage-return notification. no PCM view or samples are kept. */
        BufferPosition RecordQueuedBuffer(
            TrackPosition& track, const mc_pcm_view& pcm, double mediaStart) noexcept;

        /* the playback worker calls this when a successfully queued buffer's
        storage returns. returned storage is reusable, not necessarily audible;
        its frame count is only an estimate when device timing is unavailable.
        ignore metadata from before `Reset()`. */
        void RecordReturnedBuffer(const BufferPosition& bufferPosition) noexcept;

        /* Output calls this after stopping/flushing the old audio, before
        queueing new audio for a seek or format change. start output frames
        back at zero and make old buffer/track positions unusable. */
        void Reset() noexcept;

        /* Output calls this with the status and reading from `get_timing()`.
        return the number of output PCM frames that should have been heard,
        after device delay. if timing is unavailable or its counter restarted,
        estimate from queued duration or returned storage instead. a busy read
        returns no value, so consumers can keep their current picture/position.
        this is the same frame count for both track position and visualization. */
        std::optional<uint64_t> UpdateAudibleFrame(mc_result status, const mc_output_timing& timing) noexcept;

        /* `Output::Writer::GetPosition()` calls this with its remembered track
        position and the frame count from `UpdateAudibleFrame()`. return media seconds for
        this track, capped at its queued end. keep the previous seconds for a
        busy reading; return no value after reset until this track queues again. */
        std::optional<double> GetTrackSeconds(
            TrackPosition& track, std::optional<uint64_t> audibleFrame) const noexcept;

    private:
        uint64_t resetId = 1;
        uint64_t submittedFrames = 0;
        uint64_t completedFrames = 0;
        uint64_t lastAudibleFrame = 0;
        uint32_t sampleRate = 0;
        /* the plugin's mc_output_timing.generation value. its counter can reset
        without an Output seek/stop, so it must be checked separately. */
        std::optional<uint64_t> nativeResetId;
        bool nativeCounterRestarted = false;
    };

} } }
