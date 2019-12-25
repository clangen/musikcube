#pragma once

#include <cursespp/LayoutBase.h>
#include <cursespp/TextLabel.h>
#include <cursespp/ListWindow.h>
#include <cursespp/ITopLevelLayout.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <core/audio/PlaybackService.h>
#include <core/library/ILibrary.h>

namespace musik { namespace cube {

    class LyricsLayout:
        public cursespp::LayoutBase,
        public cursespp::ITopLevelLayout,
        public sigslot::has_slots<>
    {
        public:
            LyricsLayout(
                musik::core::audio::PlaybackService& playback,
                musik::core::ILibraryPtr library);

            virtual void OnLayout() override;
            virtual void SetShortcutsWindow(cursespp::ShortcutsWindow* w) override;
            virtual bool KeyPress(const std::string& kn) override;
            virtual void OnVisibilityChanged(bool visible) override;

        private:
            enum class State: int { NotPlaying, Loading, Loaded, Failed };

            void OnAdapterChanged(cursespp::SimpleScrollAdapter* adapter);
            void OnSelectionChanged(cursespp::ListWindow* window, size_t index, size_t prev);
            void OnPlaybackEvent(int playbackEvent);
            void OnTrackChanged(size_t index, musik::core::TrackPtr track);
            void OnLyricsLoaded(musik::core::TrackPtr track, const std::string& lyrics);

            void SetState(State state);
            void LoadLyricsForCurrentTrack();
            void UpdateAdapter(const std::string& lyrics);

            State state { State::NotPlaying };
            musik::core::ILibraryPtr library;
            musik::core::audio::PlaybackService& playback;
            std::shared_ptr<cursespp::SimpleScrollAdapter> adapter;
            std::shared_ptr<cursespp::ListWindow> listView;
            std::shared_ptr<cursespp::TextLabel> infoText;
            cursespp::ShortcutsWindow* shortcuts;
            int64_t currentTrackId;
    };

} }