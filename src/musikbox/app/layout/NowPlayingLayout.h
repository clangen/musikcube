#pragma once

#include <cursespp/LayoutBase.h>

#include <app/window/CategoryListView.h>
#include <app/window/TrackListView.h>
#include <app/window/TransportWindow.h>
#include <app/service/PlaybackService.h>

#include <core/playback/Transport.h>
#include <core/library/ILibrary.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace box {
        class NowPlayingLayout : public cursespp::LayoutBase, public sigslot::has_slots<> {
            public:
                NowPlayingLayout(
                    PlaybackService& playback,
                    musik::core::LibraryPtr library);

                virtual ~NowPlayingLayout();

                virtual void Layout();
                virtual void Show();
                virtual cursespp::IWindowPtr GetFocus();

            private:
                void InitializeWindows();
                void RequeryTrackList();

                PlaybackService& playback;
                musik::core::LibraryPtr library;
                std::shared_ptr<TrackListView> trackList;
        };
    }
}
