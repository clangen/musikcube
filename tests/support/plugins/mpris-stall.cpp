#include "mpris.h"
#include <cstdlib>
#include <unistd.h>

void MPRISRemote::LoadMetadata(mc_playback*, uint64_t, int64_t) noexcept {}
void MC_CALL MPRISRemote::PlaybackReady(void*, mc_result, mc_playback*) noexcept {}

void MPRISRemote::RefreshArtwork() noexcept {
    {
        std::lock_guard lock(this->stateMutex);
        this->artworkPending = false;
    }
    const char* ready = std::getenv("MUSIKCUBE_TEST_ARTWORK_READY");
    const char* release = std::getenv("MUSIKCUBE_TEST_ARTWORK_RELEASE");
    if (!ready || !release) {
        return;
    }
    const char signal = 'x';
    (void) write(std::atoi(ready), &signal, 1);
    char done;
    (void) read(std::atoi(release), &done, 1);
    (void) write(std::atoi(ready), &signal, 1);
}
