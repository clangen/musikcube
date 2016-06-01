#pragma once

#include <cursespp/Window.h>
#include <core/library/track/Track.h>
#include <app/service/PlaybackService.h>
#include <sigslot/sigslot.h>
#include "OutputWindow.h"

namespace musik {
    namespace box {
        class TransportWindow :
            public cursespp::Window,
#if (__clang_major__ == 7 && __clang_minor__ == 3)
            public std::enable_shared_from_this<TransportWindow>,
#endif
            public sigslot::has_slots<>
        {
            public:
                TransportWindow(musik::box::PlaybackService& playback);
                virtual ~TransportWindow();

                virtual void ProcessMessage(cursespp::IMessage &message);

                virtual void Show();
                virtual void Focus();
                virtual void Blur();

                void Update();

            private:
                void OnPlaybackServiceTrackChanged(size_t index, musik::core::TrackPtr track);
                void OnTransportVolumeChanged();
                void OnTransportTimeChanged(double time);

                bool paused, focused;
                musik::core::audio::Transport& transport;
                musik::box::PlaybackService& playback;
                musik::core::TrackPtr currentTrack;
        };
    }
}
