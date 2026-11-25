#pragma once

#include <musikcore/plugin/audio.h>
#include <algorithm>
#include <array>
#include <atomic>

/* keeps playback independent of rendering; full queues drop new input. */
class ProjectMPcmQueue {
public:
    void Write(const mc_pcm_view& pcm) noexcept {
        if (!pcm.data || !pcm.format.channels) {
            return;
        }
        for (auto& block : blocks) {
            unsigned expected = 0;
            if (!block.state.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
                continue;
            }
            block.frames = std::min(pcm.frame_count, FrameCount);
            for (size_t i = 0; i < block.frames; ++i) {
                block.samples[i * 2] = pcm.data[i * pcm.format.channels];
                block.samples[i * 2 + 1] = pcm.data[i * pcm.format.channels + (pcm.format.channels > 1 ? 1 : 0)];
            }
            block.state.store(2, std::memory_order_release);
            break;
        }
    }

    template<typename Consumer> void Drain(Consumer&& consume) noexcept {
        for (auto& block : blocks) {
            unsigned expected = 2;
            if (block.state.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
                consume(block.samples.data(), block.frames);
                block.state.store(0, std::memory_order_release);
            }
        }
    }

private:
    static constexpr size_t FrameCount = 2048;
    static constexpr size_t BlockCount = 4;

    struct Block {
        std::atomic<unsigned> state { 0 };
        std::array<float, FrameCount * 2> samples {};
        size_t frames = 0;
    };

    std::array<Block, BlockCount> blocks;
};
