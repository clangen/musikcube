#pragma once

#include <common/DeleteDefaults.h>
#include <musikcore/audio/Player.h>

#include <atomic>
#include <list>
#include <mutex>
#include <thread>

namespace musik { namespace core { namespace audio {

    /* owns the track workers for one URI player or catalog transport. a
    replaced track can still be opening a stream or returning buffers.
    Close joins those workers before their listener is destroyed. */
    class PlaybackWorkers {
    public:
        MUSIK_DELETE_COPY_AND_MOVE(PlaybackWorkers);

        explicit PlaybackWorkers(Player::EventListener& listener) : listener(listener) {}

        ~PlaybackWorkers() { this->Close(); }

        Player::Shared Start(const std::string& url, std::shared_ptr<Output> output,
            Player::Drain drain, Player::Gain gain);

        /* call outside the listener's selection lock so the workers' last
        callbacks can finish. once closed, Start cannot launch another track. */
        void Close();

    private:
        struct Worker {
            explicit Worker(Player::Shared track);

            std::weak_ptr<Player> track;
            std::atomic_bool finished { false };
            std::jthread thread;
        };

        Player::EventListener& listener;
        std::mutex mutex;
        std::list<Worker> workers;
        bool closed = false;
    };

} } }
