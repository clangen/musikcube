#pragma once

#include <sigslot/sigslot.h>

#include <cursespp/IMessageTarget.h>

#include <core/library/track/Track.h>
#include <core/playback/Transport.h>

#include <boost/thread/recursive_mutex.hpp>

namespace musik {
    namespace box {
        class PlaybackService : public cursespp::IMessageTarget, public sigslot::has_slots<> {
            public:
                sigslot::signal2<size_t, musik::core::TrackPtr> TrackChanged;

                PlaybackService(musik::core::audio::Transport& transport);

                virtual bool IsAcceptingMessages() { return true; }
                virtual void ProcessMessage(cursespp::IMessage &message);

                musik::core::audio::Transport& GetTransport() { return this->transport; }

                void Play(std::vector<musik::core::TrackPtr>& tracks, size_t index);
                void Play(size_t index);
                bool Next();
                bool Previous();
                void Stop() { transport.Stop(); }

                musik::core::TrackPtr GetTrackAtIndex(size_t index);
                size_t GetIndex();

                size_t Count() { return this->playlist.size(); }

            private:
                void OnStreamEvent(int eventType, std::string uri);

                musik::core::audio::Transport& transport;
                boost::recursive_mutex stateMutex;
                std::vector<musik::core::TrackPtr> playlist;
                size_t index, nextIndex;
        };
    }
}
