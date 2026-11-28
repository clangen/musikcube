#include "VisualizationAudioQueue.h"
#include "Visualizer.h"
#include <algorithm>
#include <chrono>
#include <new>
#include <system_error>

namespace musik { namespace core { namespace audio {
    namespace {
        constexpr size_t MaximumSamples = 1024 * 1024;
        constexpr size_t WindowFrames = 512;
        /* output clocks do not provide wakeups for audible sample boundaries.
        sample the current picture at most every 10 ms while PCM is waiting. */
        constexpr auto UpdateInterval = std::chrono::milliseconds(10);
    }

    VisualizationAudioQueue::VisualizationAudioQueue(std::function<std::optional<uint64_t>()> readAudibleFrame)
    : readAudibleFrame(std::move(readAudibleFrame)) {
    }

    VisualizationAudioQueue::~VisualizationAudioQueue() {
        {
            std::unique_lock lock(this->mutex);
            this->quit = true;
            this->changed.notify_one();
        }
        if (this->worker.joinable()) {
            this->worker.join();
        }
    }

    void VisualizationAudioQueue::Enqueue(const mc_pcm_view& pcm, uint64_t firstFrame) noexcept {
        if (!vis::HasAudioConsumers() || pcm.frame_count > MaximumSamples / pcm.format.channels) {
            return;
        }
        std::unique_lock lock(this->mutex);
        const auto result = this->Capture(pcm, firstFrame);
        if (result != MC_OK && !this->captureWarning) {
            this->captureWarning = true;
            lock.unlock();
            vis::ReportCaptureFailure(result);
        }
    }

    mc_result VisualizationAudioQueue::Capture(const mc_pcm_view& pcm, uint64_t firstFrame) noexcept {
        if (!this->worker.joinable()) {
            /* capture is optional after audio admission. a thread resource
            failure must not turn accepted PCM into a failed audio write. */
            try {
                this->worker = std::thread([this] {
                    this->Run();
                });
            }
            catch (const std::bad_alloc&) {
                return MC_ERROR_INTERNAL;
            }
            catch (const std::system_error&) {
                return MC_ERROR_IO;
            }
        }
        const auto count = pcm.frame_count * pcm.format.channels;
        /* bound the retained audio when visualization falls behind. discard
        the oldest pictures, keeping the latest accepted audio. */
        while (!this->pendingBuffers.empty() && this->queuedSamples + count > MaximumSamples) {
            this->queuedSamples -= this->pendingBuffers.front().samples.size();
            this->pendingBuffers.pop_front();
        }
        try {
            this->pendingBuffers.push_back({
                .samples = std::vector<float>(pcm.data, pcm.data + count),
                .format = pcm.format,
                .firstFrame = firstFrame,
            });
        }
        catch (const std::bad_alloc&) {
            return MC_ERROR_INTERNAL;
        }
        this->queuedSamples += count;
        this->changed.notify_one();
        return MC_OK;
    }

    void VisualizationAudioQueue::Reset() {
        std::unique_lock lock(this->mutex);
        this->pendingBuffers.clear();
        this->queuedSamples = 0;
        ++this->epoch;
        this->deliveredThrough.reset();
        this->captureWarning = false;
        this->paused = false;
        this->changed.notify_one();
    }

    void VisualizationAudioQueue::SetPaused(bool paused) {
        std::unique_lock lock(this->mutex);
        this->paused = paused;
        this->changed.notify_one();
    }

    void VisualizationAudioQueue::Run() {
        std::unique_lock lock(this->mutex);
        while (!this->quit) {
            this->changed.wait(lock, [this] {
                return this->quit || (!this->paused && !this->pendingBuffers.empty());
            });
            if (this->quit) {
                break;
            }
            const auto epoch = this->epoch;
            /* asking the output can take its lock. release ours so a seek can
            discard captured audio without the two locks blocking each other. */
            lock.unlock();
            const auto audibleFrame = this->readAudibleFrame();
            lock.lock();
            if (epoch != this->epoch || this->paused || this->quit) {
                continue;
            }
            if (audibleFrame && !this->pendingBuffers.empty()) {
                /* keep the newest buffer containing already audible samples.
                older pictures are irrelevant, even after a long scheduling gap. */
                while (this->pendingBuffers.size() > 1 && this->pendingBuffers[1].firstFrame < *audibleFrame) {
                    this->queuedSamples -= this->pendingBuffers.front().samples.size();
                    this->pendingBuffers.pop_front();
                }
                auto& pending = this->pendingBuffers.front();
                if (*audibleFrame > pending.firstFrame) {
                    const auto channels = pending.format.channels;
                    const auto frames = pending.samples.size() / channels;
                    const auto end = std::min<uint64_t>(frames, *audibleFrame - pending.firstFrame);
                    const auto outputEnd = pending.firstFrame + end;
                    if (end >= std::min(WindowFrames, frames) &&
                        (!this->deliveredThrough || outputEnd > *this->deliveredThrough))
                    {
                        const auto count = std::min<uint64_t>(WindowFrames, end);
                        const mc_pcm_view pcm {
                            .struct_size = sizeof(mc_pcm_view),
                            .data = pending.samples.data() + (end - count) * channels,
                            .frame_count = count,
                            .format = pending.format,
                        };
                        /* reset waits for this one delivery, so discarded
                        pre-seek audio cannot arrive after reset returns. */
                        vis::DeliverAudio(pcm);
                        this->deliveredThrough = outputEnd;
                    }
                    if (end == frames) {
                        this->queuedSamples -= pending.samples.size();
                        this->pendingBuffers.pop_front();
                    }
                }
            }
            /* one picture per interval, never a burst of overdue pictures.
            waiting releases the mutex for captures, pause, and reset. */
            this->changed.wait_for(lock, UpdateInterval, [this, epoch] {
                return this->quit || this->paused || epoch != this->epoch;
            });
        }
    }

} } }
