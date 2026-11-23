#include "pch.hpp"

#include <common/Contract.h>
#include <musikcore/audio/PlaybackWorkers.h>
#include <utility>

namespace musik { namespace core { namespace audio {

    Player::Shared PlaybackWorkers::Start(const std::string& url, std::shared_ptr<Output> output,
        Player::Drain drain, Player::Gain gain)
    {
        musik::contract::Assert(output != nullptr, "a playback worker requires an output");
        auto player = Player::Shared(new Player(url, std::move(output), drain, this->listener, gain));
        std::lock_guard lock(this->mutex);
        if (this->closed) {
            return {};
        }

        /* a finished worker has returned from its last callback. joining it
        here keeps long playlists from accumulating thread handles. */
        this->workers.remove_if([](const Worker& worker) { return worker.finished.load(); });
        this->workers.emplace_back(player);
        return player;
    }

    PlaybackWorkers::Worker::Worker(Player::Shared track)
    : track(track)
    , thread([this, track = std::move(track)]() mutable {
        track->Run();
        track.reset();
        this->finished.store(true);
    })
    {
    }

    void PlaybackWorkers::Close() {
        std::list<Worker> retiring;
        {
            std::lock_guard lock(this->mutex);
            this->closed = true;
            retiring.splice(retiring.end(), this->workers);
        }

        /* interrupt every track before joining any of them. a worker may
        be blocked in a source read or writing to a shared device. */
        for (auto& worker : retiring) {
            if (auto track = worker.track.lock()) {
                track->RequestStop();
                track->output->Stop();
            }
        }
        for (auto& worker : retiring) {
            worker.thread.join();
        }
    }

} } }
