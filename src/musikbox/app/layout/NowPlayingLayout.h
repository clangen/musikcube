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
        class NowPlayingLayout :
            public cursespp::LayoutBase,
#if (__clang_major__ == 7 && __clang_minor__ == 3)
            public std::enable_shared_from_this<NowPlayingLayout>,
#endif
            public sigslot::has_slots<>
        {
            public:
                NowPlayingLayout(
                    PlaybackService& playback,
                    musik::core::LibraryPtr library);

                virtual ~NowPlayingLayout();

                virtual void Layout();
                virtual void OnVisibilityChanged(bool visible);
                virtual cursespp::IWindowPtr GetFocus();
                virtual bool KeyPress(const std::string& key);

            private:
                void InitializeWindows();
                void RequeryTrackList();

                PlaybackService& playback;
                musik::core::LibraryPtr library;
                std::shared_ptr<TrackListView> trackList;
        };
    }
}
