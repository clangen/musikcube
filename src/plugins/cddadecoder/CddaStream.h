#pragma once

#include <musikcore/plugin/stream.h>
#include <common/FixedRingQueue.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct CddaTrackInfo {
    unsigned number = 0;
    uint32_t startFrame = 0;
    uint32_t endFrame = 0;
    bool audio = false;
};

/* Table-of-contents data shared with the indexer and metadata lookup. */
struct CddaDiscInfo {
    char driveLetter = 0;
    std::vector<CddaTrackInfo> tracks;
};

/* One track stream owns its drive handle. The worker performs overlapped I/O;
Interrupt can cancel it from another thread and remains latched before Open. */
class CddaStream {
public:
    CddaStream();
    ~CddaStream();
    CddaStream(const CddaStream&) = delete;
    CddaStream& operator=(const CddaStream&) = delete;

    static bool CanHandle(const mc_media_hint& hint);
    mc_result Open(mc_string_view uri, mc_stream_open_flags flags);
    /* Read only the disc layout, without starting the audio reader. */
    mc_result Inspect(mc_string_view uri);
    mc_result Close();
    void Interrupt() noexcept;
    mc_result Info(mc_stream_info& info) const;
    mc_result Read(void* destination, size_t capacity, size_t& bytes, mc_read_state& state);
    mc_result Seek(uint64_t offset);
    mc_result Tell(uint64_t& offset) const;
    const CddaDiscInfo& Disc() const { return this->disc; }

private:
    mc_result Control(DWORD code, void* input, DWORD inputSize, void* output, DWORD outputSize, DWORD& bytes,
        uint64_t generation = UINT64_MAX);
    mc_result OpenTrack(mc_string_view uri, mc_stream_open_flags flags);
    void ReadAhead() noexcept;
    mc_result WaitForReserve(std::unique_lock<std::mutex>& lock, uint64_t generation, uint64_t offset);
    static constexpr size_t SectorBytes = 2352;
    static constexpr size_t CacheSectors = 8;
    /* Each block holds about 107 ms of CD audio. Start with a smaller reserve
    for responsiveness, then fill the larger queue to absorb slow drive reads. */
    static constexpr size_t QueueBlocks = 96;
    static constexpr size_t ReserveBlocks = 32;

    struct ReadBlock {
        uint64_t offset = 0;
        size_t size = 0;
        std::array<unsigned char, SectorBytes * CacheSectors> data {};
    };

    std::mutex handleMutex;
    HANDLE drive = INVALID_HANDLE_VALUE;
    HANDLE cancelEvent = nullptr;
    std::atomic<bool> cancelled { false };
    std::string uri;
    CddaDiscInfo disc;
    uint64_t position = 0;
    uint64_t length = 0;
    uint32_t firstSector = 0;
    /* The reader consumes completed blocks while the worker fills the bounded
    queue. A generation change invalidates work submitted before a seek. */
    std::mutex queueMutex;
    std::condition_variable queueChanged;
    musik::FixedRingQueue<std::unique_ptr<ReadBlock>, QueueBlocks> blocks;
    std::thread worker;
    std::atomic<uint64_t> generation { 0 };
    uint64_t nextRead = 0;
    mc_result readError = MC_OK;
};
