#pragma once

#include <sigslot/sigslot.h>

#include <cursespp/IMessageTarget.h>

#include <core/library/track/Track.h>
#include <core/playback/Transport.h>

using musik::core::TrackPtr;
using musik::core::audio::Transport;

using cursespp::IMessageTarget;
using cursespp::IMessage;

namespace musik {
    namespace box {
        class PlaybackService : public IMessageTarget, public sigslot::has_slots<> {
            public:
                PlaybackService(Transport& transport);

                virtual bool IsAcceptingMessages() { return true; }
                virtual void ProcessMessage(IMessage &message);

                Transport& GetTransport() { return this->transport; }

                void Play(std::vector<TrackPtr>& tracks, size_t index);
                void Play(size_t index);
                bool Next();
                bool Previous();
                void Stop() { transport.Stop(); }

                size_t Count() { return this->playlist.size(); }

            private:
                void OnStreamEvent(int eventType, std::string uri);

                Transport& transport;
                std::vector<TrackPtr> playlist;
                size_t index;
        };
    }
}
