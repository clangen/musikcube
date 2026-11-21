#pragma once

#include <cursespp/LayoutBase.h>
#include <cursespp/TextLabel.h>
#include <cursespp/ListWindow.h>
#include <cursespp/ITopLevelLayout.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <musikcore/playback.hpp>
#include <app/util/AsyncCallbackScope.h>

namespace musik { namespace cube {

    class LyricsLayout:
        public cursespp::LayoutBase,
        public cursespp::ITopLevelLayout,
        public sigslot::has_slots<>
    {
        public:
            DELETE_CLASS_DEFAULTS(LyricsLayout)

            explicit LyricsLayout(musik::core::api::Context& musikcore);

            /* IWindow */
            void OnLayout() override;
            void SetShortcutsWindow(cursespp::ShortcutsWindow* w) override;
            bool KeyPress(const std::string& kn) override;
            void OnVisibilityChanged(bool visible) override;

        private:
            enum class State: int { NotPlaying, Loading, Loaded, Failed };

            void OnLyricsLoaded(std::string lyrics);

            void SetState(State state);
            void LoadLyricsForCurrentTrack();
            void UpdateAdapter();

            State state { State::NotPlaying };
            musik::core::api::Context& musikcore;
            musik::core::api::Playback playback;
            AsyncCallbackScope lyricsWork;
            std::shared_ptr<cursespp::SimpleScrollAdapter> adapter;
            std::shared_ptr<cursespp::ListWindow> listView;
            std::shared_ptr<cursespp::TextLabel> infoText;
            cursespp::ShortcutsWindow* shortcuts;
            int64_t currentTrackId;
            std::string currentLyrics;
            std::string trackTitle, trackArtist;
    };

} }