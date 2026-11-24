#pragma once

#include <windows.h>
#include <atomic>
#include <cstdint>

/* A drive that cannot be inspected is not necessarily empty. Windows can
report NOT_READY while media is initializing, so preserve indexed entries
unless the media is known to have been removed or no track exists. */
inline bool CddaDriveTypeUncertain(UINT type) noexcept {
    return type == DRIVE_UNKNOWN;
}

inline bool CddaFindFailureUncertain(DWORD error, bool removalObserved) noexcept {
    return error != ERROR_FILE_NOT_FOUND && error != ERROR_NO_MEDIA_IN_DRIVE && !removalObserved;
}

/* Pair volume removal bits with a notification generation. A successful scan
can clear old removal evidence only if no notification raced the scan. */
class CddaVolumeChanges {
public:
    uint64_t Snapshot() const noexcept { return this->state.load(); }

    bool Removed(char letter) const noexcept {
        return letter >= 'A' && letter <= 'Z' &&
            (this->state.load() & (uint64_t { 1 } << (letter - 'A'))) != 0;
    }

    void ConfirmPresent(char letter, uint64_t snapshot) noexcept {
        if (letter >= 'A' && letter <= 'Z') {
            const auto updated = snapshot & ~(uint64_t { 1 } << (letter - 'A'));
            this->state.compare_exchange_strong(snapshot, updated);
        }
    }

    void Notify(uint32_t mask, bool removed) noexcept {
        auto previous = this->state.load();
        uint64_t updated;
        do {
            const auto oldMask = static_cast<uint32_t>(previous);
            const auto newMask = removed ? oldMask | mask : oldMask & ~mask;
            updated = ((previous + (uint64_t { 1 } << 32)) & 0xffffffff00000000ULL) | newMask;
        } while (!this->state.compare_exchange_weak(previous, updated));
    }

private:
    std::atomic<uint64_t> state { 0 };
};
