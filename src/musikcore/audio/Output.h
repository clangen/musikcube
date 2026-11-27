#pragma once

#include <common/DeleteDefaults.h>
#include <musikcore/ffi/plugin_abi/PluginRegistry.h>
#include <musikcore/audio/Buffer.h>
#include <musikcore/plugin/services.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

namespace musik { namespace core { namespace audio {

    /* lets players send audio through a C output plugin. Output checks the
    audio format, prepares the device, and tracks buffers until the plugin
    returns them. gapless tracks share an Output to keep the device playing
    across a track change. Stop interrupts a blocked write or drain.
    destroy the Output after its writers finish and before the plugin unloads.
    neither copyable nor movable. */
    class Output {
        static constexpr size_t CompletionCapacity = 64;
        static_assert(CompletionCapacity <= UINT8_MAX + 1u, "slot indices must fit in a byte");

    public:
        /* the player gets each buffer back on its own worker. the outcome
        tells it whether to process the samples or just recycle the buffer. */
        struct CompletionSink {
            virtual ~CompletionSink() = default;
            virtual void CompleteBuffer(musik::core::audio::Buffer*, mc_result) = 0;
        };

        struct Device {
            std::string id;
            std::string name;
        };

        /* every Player owns a Writer. its playback loop calls TryQueueAudioBuffer
        to send decoded samples to the output plugin. the plugin may still be
        reading those samples when the call returns. the Writer tracks which
        buffers this Player sent, so ProcessCompletions can hand them back to
        the Player for reuse once the plugin has finished with them.

        the Writer belongs to the track because the Output can outlive it.
        for example, gapless playback keeps the same device open for the next
        track. Revoke lets the old Player collect its last buffers while
        preventing a late write or seek from disturbing the new track. */
        class Writer {
        public:
            MUSIK_DELETE_COPY_AND_MOVE(Writer);

            /* the track's worker owns this writer. output and sink must outlive it. */
            Writer(Output& output, CompletionSink& sink);
            ~Writer();

            /* on MC_ERROR_QUEUE_FULL the worker keeps the buffer and can retry it. */
            mc_result TryQueueAudioBuffer(musik::core::audio::Buffer& buffer);
            /* after MC_ERROR_QUEUE_FULL, wait for another chance to queue the
            same buffer. a seek or stop can interrupt the wait. */
            void WaitForQueueSpace() noexcept { this->WaitForBufferChange(); }

            /* deliver returned buffers to the sink. keep doing this until
            HasPending is false before destroying the writer. */
            void ProcessCompletions();
            bool HasPending() const noexcept;

            /* when the worker needs storage back, wait before checking
            ProcessCompletions again. a playback command can also wake it. */
            void WaitForReturnedBuffer() noexcept { this->WaitForBufferChange(); }

            mc_result ResetForSeek();
            mc_result PrewarmOutput(const mc_audio_format& format, size_t frames);
            mc_result Drain();
            mc_result StopAfterFailure();
            /* the transport can call this from another thread. calls already
            in progress can finish. resuming the output never reopens this writer. */
            void Revoke() noexcept;

        private:
            void WaitForBufferChange() noexcept;
            bool PrioritizeFailedCompletion() noexcept;

            Output& output;
            CompletionSink& sink;
            std::atomic<bool> revoked { false };
            uint64_t lastWakeSeen;
            uint64_t lastFailureSeen;
            /* the worker keeps its own submission order even when another
            track frees an earlier slot in the shared output. */
            std::array<uint8_t, CompletionCapacity> pendingSlots;
            size_t firstPending = 0;
            size_t pendingCount = 0;
        };

        MUSIK_DELETE_COPY_AND_MOVE(Output);

        Output() = default;
        Output(ffi::PluginRegistry::Factory<mc_output_factory> factory, std::string id, std::string name,
            std::string device, mc_plugin_settings_service settings = MC_PLUGIN_SETTINGS_SERVICE_DEFAULT);
        ~Output();

        static size_t InstanceCount() noexcept;

        const char* Name() const { return this->name.c_str(); }
        const std::string& Id() const { return this->id; }
        std::vector<Device> Devices() const;
        std::string PinnedDeviceId() const;
        bool SetDefaultDevice(const char* device);
        int GetDefaultSampleRate();

        mc_result Pause();
        mc_result Resume();
        mc_result SetVolume(double volume);
        double GetVolume() const;
        void Stop();
        void FlushForReplacement();
        double Latency() const;

        void NotifyWriters() noexcept;

    private:
        struct Completion {
            /* while the plugin holds a buffer, this slot contains its token.
            the callback replaces that token with Returning before writing
            the outcome, then sets Returned so the worker can read it.
            submission tokens start at CompletionCapacity, above these markers. */
            static constexpr uint64_t Empty = 0;
            static constexpr uint64_t Returning = 1;
            static constexpr uint64_t Returned = 2;
            std::atomic<uint64_t> token { Empty };
            musik::core::audio::Buffer* buffer = nullptr;
            mc_result outcome = MC_OK;
        };

        static_assert(CompletionCapacity > Completion::Returned);
        static void MC_CALL Completed(void*, uint64_t, mc_result) noexcept;
        static void MC_CALL Ready(void*) noexcept;
        mc_result Prepare(const mc_pcm_view& pcm, const std::atomic_bool& revoked);
        mc_result EnsureInstance();
        mc_string_view PlaybackDeviceId() const;
        std::unique_lock<std::timed_mutex> LockForControl();
        mc_result SetPaused(bool pause);
        mc_result ApplyVolume();
        void StopBackend();
        mc_result FlushBackend();

        ffi::PluginRegistry::Factory<mc_output_factory> factory;
        std::string id;
        std::string name = "NoOutput";
        std::string device;
        mc_plugin_settings_service settings = MC_PLUGIN_SETTINGS_SERVICE_DEFAULT;
        mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
        mutable std::timed_mutex ordinary;
        std::atomic<unsigned> waitingControls { 0 };
        std::mutex lifecycle;
        std::atomic<bool> stopped { true };
        std::atomic<bool> prepared { false };
        std::atomic<double> volume { 1.0 };
        double deviceVolume = 1.0;
        bool paused = false;
        void* instance = nullptr;
        const mc_output_vtable* api = nullptr;
        mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
        size_t capacity = 0;
        uint64_t nextToken = 1;
        std::array<Completion, CompletionCapacity> completions;
        std::atomic<uint64_t> completionFailures { 0 };
        std::atomic<uint64_t> wakeCount { 0 };
    };

} } }
