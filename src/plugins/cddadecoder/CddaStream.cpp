#include "CddaStream.h"
#include <winioctl.h>
#include <ntddcdrm.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace {
    constexpr uint32_t SectorsPerMinute = 60 * 75;
    uint32_t Sector(const UCHAR (&address)[4]) {
        return address[1] * SectorsPerMinute + address[2] * 75 + address[3];
    }

    bool ParseTrack(std::string_view uri, char& drive, unsigned& track) {
        if ((uri.size() != 14 && uri.size() != 9) || !std::isalpha(static_cast<unsigned char>(uri[0])) ||
            uri[1] != ':' || (uri[2] != '\\' && uri[2] != '/'))
        {
            return false;
        }

        /* Windows names these Track01.cda; older library entries use 01.cda. */
        const size_t numberOffset = uri.size() == 14 ? 8 : 3;
        if ((numberOffset == 8 && _strnicmp(uri.data() + 3, "Track", 5) != 0) ||
            !std::isdigit(static_cast<unsigned char>(uri[numberOffset])) ||
            !std::isdigit(static_cast<unsigned char>(uri[numberOffset + 1])) ||
            _strnicmp(uri.data() + numberOffset + 2, ".cda", 4) != 0)
        {
            return false;
        }

        drive = static_cast<char>(std::toupper(static_cast<unsigned char>(uri[0])));
        track = (uri[numberOffset] - '0') * 10 + uri[numberOffset + 1] - '0';
        return track != 0;
    }
}

CddaStream::CddaStream()
: cancelEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {
    if (!this->cancelEvent) {
        throw std::runtime_error("cannot create CD cancellation event");
    }
}

CddaStream::~CddaStream() {
    this->Close();
    if (!CloseHandle(this->cancelEvent)) {
        std::fputs("CDDA stream: cannot close cancellation event\n", stderr);
        std::abort();
    }
}

bool CddaStream::CanHandle(const mc_media_hint& hint) {
    char drive = 0;
    unsigned track = 0;
    return ParseTrack(std::string_view(hint.uri.data ? hint.uri.data : "", hint.uri.length), drive, track);
}

mc_result CddaStream::Open(mc_string_view value, mc_stream_open_flags flags) {
    return this->OpenTrack(value, flags);
}

mc_result CddaStream::Inspect(mc_string_view value) {
    return this->OpenTrack(value, MC_STREAM_OPEN_READ);
}

mc_result CddaStream::OpenTrack(mc_string_view value, mc_stream_open_flags flags) {
    if (this->cancelled.load()) {
        return MC_ERROR_CANCELLED;
    }
    if (flags != MC_STREAM_OPEN_READ) {
        return MC_ERROR_UNSUPPORTED;
    }
    char letter = 0;
    unsigned track = 0;
    const std::string_view view(value.data ? value.data : "", value.length);
    if (!ParseTrack(view, letter, track)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    /* A .cda file is a track reference, not the audio data. Open the device
    itself and use its table of contents to find the raw sector range. */
    std::string path = "\\\\.\\A:";
    path[4] = letter;
    HANDLE handle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return MC_ERROR_IO;
    }
    {
        std::lock_guard lock(this->handleMutex);
        this->drive = handle;
    }
    if (this->cancelled.load()) {
        return MC_ERROR_CANCELLED;
    }

    CDROM_TOC toc {};
    DWORD received = 0;
    const auto result = this->Control(IOCTL_CDROM_READ_TOC, nullptr, 0, &toc, sizeof(toc), received);
    if (result != MC_OK) {
        return result;
    }
    if (track < toc.FirstTrack || track > toc.LastTrack) {
        return MC_ERROR_NOT_FOUND;
    }

    /* Keep the complete layout so the indexer can identify the disc without
    issuing a separate device request for every track. */
    if (!toc.FirstTrack || toc.LastTrack < toc.FirstTrack || toc.LastTrack > 99) {
        return MC_ERROR_IO;
    }
    const size_t trackCount = toc.LastTrack - toc.FirstTrack + 1;
    if (received < offsetof(CDROM_TOC, TrackData) + (trackCount + 1) * sizeof(TRACK_DATA)) {
        return MC_ERROR_IO;
    }
    this->disc.driveLetter = letter;
    this->disc.tracks.clear();
    this->disc.tracks.reserve(trackCount);
    for (size_t i = 0; i < trackCount; ++i) {
        const uint32_t start = Sector(toc.TrackData[i].Address);
        const uint32_t finish = Sector(toc.TrackData[i + 1].Address);
        if (start < 150 || finish <= start) {
            return MC_ERROR_IO;
        }
        this->disc.tracks.push_back(
            { static_cast<unsigned>(toc.FirstTrack + i), start, finish, (toc.TrackData[i].Control & 4) == 0 });
    }

    const auto index = track - toc.FirstTrack;
    if (toc.TrackData[index].Control & 4) {
        return MC_ERROR_UNSUPPORTED;
    }
    const uint32_t begin = Sector(toc.TrackData[index].Address);
    const uint32_t end = Sector(toc.TrackData[index + 1].Address);
    if (begin < 150 || end <= begin) {
        return MC_ERROR_IO;
    }
    this->firstSector = begin - 150;
    this->length = static_cast<uint64_t>(end - begin) * SectorBytes;
    this->uri.assign(view);

    return MC_OK;
}

mc_result CddaStream::Control(DWORD code, void* input, DWORD inputSize, void* output, DWORD outputSize, DWORD& bytes,
    uint64_t expectedGeneration)
{
    const auto obsolete = [&] {
        return expectedGeneration != UINT64_MAX && this->generation.load() != expectedGeneration;
    };
    if (this->cancelled.load() || obsolete()) {
        return MC_ERROR_CANCELLED;
    }
    OVERLAPPED operation {};
    operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!operation.hEvent) {
        return MC_ERROR_IO;
    }

    /* The cancel event covers interruption both before and after submission.
    Even after cancellation, retain the OVERLAPPED and its buffers until the
    device reports completion. */
    BOOL success = DeviceIoControl(this->drive, code, input, inputSize, output, outputSize, &bytes, &operation);
    DWORD error = success ? ERROR_SUCCESS : GetLastError();
    bool waitFailed = false;
    if (!success && error == ERROR_IO_PENDING) {
        /* A seek can race between the generation check and submission. */
        if (obsolete()) {
            CancelIoEx(this->drive, &operation);
        }
        const HANDLE events[] { this->cancelEvent, operation.hEvent };
        const DWORD wait = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        waitFailed = wait != WAIT_OBJECT_0 && wait != WAIT_OBJECT_0 + 1;
        if (wait != WAIT_OBJECT_0 + 1 || this->cancelled.load()) {
            CancelIoEx(this->drive, &operation);
        }
        success = GetOverlappedResult(this->drive, &operation, &bytes, TRUE);
        error = success ? ERROR_SUCCESS : GetLastError();
    }
    if (!CloseHandle(operation.hEvent)) {
        std::fputs("CDDA stream: cannot close I/O event\n", stderr);
        std::abort();
    }
    if (this->cancelled.load() || obsolete() || error == ERROR_OPERATION_ABORTED) {
        return MC_ERROR_CANCELLED;
    }
    return success && !waitFailed ? MC_OK : MC_ERROR_IO;
}

void CddaStream::Interrupt() noexcept {
    /* Publish cancellation under the waiters' mutex so notification cannot
    slip between a predicate check and the condition-variable wait. */
    {
        std::lock_guard lock(this->queueMutex);
        this->cancelled.store(true);
    }
    if (!SetEvent(this->cancelEvent)) {
        std::fputs("CDDA stream: cannot signal cancellation\n", stderr);
        std::abort();
    }
    this->queueChanged.notify_all();
    std::lock_guard lock(this->handleMutex);
    if (this->drive != INVALID_HANDLE_VALUE) {
        CancelIoEx(this->drive, nullptr);
    }
}

mc_result CddaStream::Close() {
    if (this->worker.joinable()) {
        this->Interrupt();
        this->worker.join();
    }
    std::lock_guard lock(this->handleMutex);
    if (this->drive != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(this->drive)) {
            std::fputs("CDDA stream: cannot close device handle\n", stderr);
            std::abort();
        }
        this->drive = INVALID_HANDLE_VALUE;
    }
    return MC_OK;
}

mc_result CddaStream::Info(mc_stream_info& info) const {
    info.flags = MC_STREAM_READABLE | MC_STREAM_SEEKABLE | MC_STREAM_LENGTH_KNOWN;
    info.length_bytes = this->length;
    info.uri = { this->uri.data(), this->uri.size() };
    info.extension = { "cda", 3 };
    return MC_OK;
}

mc_result CddaStream::Read(void* destination, size_t capacity, size_t& bytes, mc_read_state& state) {
    bytes = 0;
    state = MC_READ_MORE;
    auto* output = static_cast<unsigned char*>(destination);

    std::unique_lock lock(this->queueMutex);
    if (this->cancelled.load()) {
        return MC_ERROR_CANCELLED;
    }
    if (capacity && this->position < this->length && !this->worker.joinable()) {
        /* Players open their next track early. Wait until audio is actually
        requested before moving the drive head or starting read-ahead. */
        this->worker = std::thread([this] { this->ReadAhead(); });
        const auto result = this->WaitForReserve(lock, this->generation.load(), this->position);
        if (result != MC_OK) {
            return result;
        }
    }
    while (bytes < capacity && this->position < this->length) {
        this->queueChanged.wait(lock, [this] {
            return this->cancelled.load() || this->readError != MC_OK || !this->blocks.Empty();
        });
        if (this->cancelled.load()) {
            return MC_ERROR_CANCELLED;
        }
        if (this->readError != MC_OK) {
            return this->readError;
        }

        const auto& block = **this->blocks.Front();
        if (this->position < block.offset || this->position >= block.offset + block.size) {
            return MC_ERROR_IO;
        }

        const auto offset = static_cast<size_t>(this->position - block.offset);
        const auto available = std::min(capacity - bytes, block.size - offset);
        std::memcpy(output + bytes, block.data.data() + offset, available);
        bytes += available;
        this->position += available;

        if (this->position == block.offset + block.size) {
            this->blocks.TryPop();
            this->queueChanged.notify_all();
        }
    }
    if (this->position == this->length) {
        state = MC_READ_END;
    }
    return MC_OK;
}

void CddaStream::ReadAhead() noexcept {
    try {
        std::unique_lock lock(this->queueMutex);
        while (!this->cancelled.load()) {
            this->queueChanged.wait(lock, [this] {
                return this->cancelled.load() ||
                    (this->readError == MC_OK && this->blocks.Size() < QueueBlocks && this->nextRead < this->length);
            });
            if (this->cancelled.load()) {
                break;
            }

            const auto offset = this->nextRead;
            const auto currentGeneration = this->generation.load();
            const auto count = static_cast<DWORD>(
                std::min<uint64_t>(CacheSectors, (this->length - offset) / SectorBytes));
            lock.unlock();

            auto block = std::make_unique<ReadBlock>();
            block->offset = offset;
            RAW_READ_INFO request {};
            request.DiskOffset.QuadPart = static_cast<LONGLONG>(this->firstSector + offset / SectorBytes) * 2048;
            request.SectorCount = count;
            request.TrackMode = CDDA;

            DWORD received = 0;
            const auto result = this->Control(IOCTL_CDROM_RAW_READ, &request, sizeof(request),
                block->data.data(), count * SectorBytes, received, currentGeneration);

            lock.lock();
            if (this->cancelled.load()) {
                break;
            }
            if (currentGeneration != this->generation.load()) {
                continue;
            }
            if (result != MC_OK || received != count * SectorBytes) {
                this->readError = result == MC_OK ? MC_ERROR_IO : result;
                this->queueChanged.notify_all();
                continue;
            }

            block->size = received;
            if (!this->blocks.TryPush(std::move(block))) {
                std::fputs("CDDA stream: read-ahead queue invariant failed\n", stderr);
                std::abort();
            }
            this->nextRead += received;
            this->queueChanged.notify_all();
        }
    }
    catch (...) {
        std::lock_guard lock(this->queueMutex);
        this->readError = MC_ERROR_INTERNAL;
        this->queueChanged.notify_all();
    }
}

mc_result CddaStream::WaitForReserve(std::unique_lock<std::mutex>& lock, uint64_t expected, uint64_t offset) {
    const auto aligned = (offset / SectorBytes) * SectorBytes;
    const auto remaining = this->length - aligned;
    const auto blockBytes = SectorBytes * CacheSectors;
    const auto needed = static_cast<size_t>(std::min<uint64_t>(ReserveBlocks,
        (remaining + blockBytes - 1) / blockBytes));

    this->queueChanged.wait(lock, [&] {
        return this->cancelled.load() || this->readError != MC_OK ||
            this->generation.load() != expected || this->blocks.Size() >= needed;
    });
    if (this->cancelled.load() || this->generation.load() != expected) {
        return MC_ERROR_CANCELLED;
    }
    return this->readError;
}

mc_result CddaStream::Seek(uint64_t offset) {
    if (offset > this->length) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    std::unique_lock lock(this->queueMutex);
    if (this->cancelled.load()) {
        return MC_ERROR_CANCELLED;
    }

    /* Cancel the old read while holding the queue lock: the worker cannot
    submit a new-generation request until the old cancellation is issued. */
    const auto currentGeneration = ++this->generation;
    while (this->blocks.TryPop()) {}
    this->nextRead = (offset / SectorBytes) * SectorBytes;
    this->readError = MC_OK;
    this->position = offset;
    {
        std::lock_guard handleLock(this->handleMutex);
        if (this->drive != INVALID_HANDLE_VALUE) {
            CancelIoEx(this->drive, nullptr);
        }
    }
    this->queueChanged.notify_all();

    /* Opening or seeking an upcoming track must not move the drive head. */
    if (!this->worker.joinable()) {
        return MC_OK;
    }

    /* Keep the output stopped while the head moves and a reserve accumulates. */
    return this->WaitForReserve(lock, currentGeneration, offset);
}

mc_result CddaStream::Tell(uint64_t& offset) const {
    offset = this->position;
    return MC_OK;
}
