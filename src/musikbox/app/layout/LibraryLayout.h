#pragma once

#include <cursespp/LayoutBase.h>
#include <cursespp/LayoutStack.h>

#include <app/layout/BrowseLayout.h>
#include <app/layout/NowPlayingLayout.h>
#include <app/window/TransportWindow.h>
#include <app/service/PlaybackService.h>

#include <core/playback/Transport.h>
#include <core/library/ILibrary.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace box {
        class LibraryLayout : public cursespp::LayoutBase, public sigslot::has_slots<> {
            public:
                LibraryLayout(
                    PlaybackService& playback,
                    musik::core::LibraryPtr library);

                virtual ~LibraryLayout();

                virtual void Layout();

                virtual cursespp::IWindowPtr FocusNext();
                virtual cursespp::IWindowPtr FocusPrev();
                virtual cursespp::IWindowPtr GetFocus();

                virtual bool KeyPress(const std::string& key);

            private:
                void InitializeWindows();
                void ShowNowPlaying();
                void ShowBrowse();
                void ChangeMainLayout(std::shared_ptr<cursespp::LayoutBase> newLayout);

                PlaybackService& playback;
                musik::core::audio::Transport& transport;
                musik::core::LibraryPtr library;
                std::shared_ptr<BrowseLayout> browseLayout;
                std::shared_ptr<TransportWindow> transportView;
                std::shared_ptr<NowPlayingLayout> nowPlayingLayout;
                std::shared_ptr<cursespp::LayoutBase> visibleLayout;
        };
    }
}
