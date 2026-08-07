#pragma once

/**
 * @file LyricsLayout.h
 * @brief Layout that displays the lyrics of the currently playing track.
 * @details Loads lyrics for the current track and shows them in a scrollable
 *          list, updating as the playing track changes and reporting loading
 *          and error states.
 */

#include <cursespp/LayoutBase.h>
#include <cursespp/TextLabel.h>
#include <cursespp/ListWindow.h>
#include <cursespp/ITopLevelLayout.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/ILibrary.h>

namespace musik { namespace cube {

    /**
     * @brief Lyrics display layout.
     * @details Fetches and renders the lyrics for the current track in a
     *          scrollable list, tracking playback changes and showing
     *          NotPlaying, Loading, Loaded and Failed states.
     */
    class LyricsLayout:
        public cursespp::LayoutBase,
        public cursespp::ITopLevelLayout,
        public sigslot::has_slots<>
    {
        public:
            DELETE_CLASS_DEFAULTS(LyricsLayout)

            /**
             * @brief Creates the lyrics layout.
             * @param playback the playback service to follow
             * @param library the library used to load lyrics
             */
            LyricsLayout(
                musik::core::audio::PlaybackService& playback,
                musik::core::ILibraryPtr library);

            /* IWindow */
            /**
             * @brief Positions and lays out the child windows.
             */
            void OnLayout() override;
            /**
             * @brief Attaches the shortcuts window shown at the bottom.
             * @param w the shortcuts window
             */
            void SetShortcutsWindow(cursespp::ShortcutsWindow* w) override;
            /**
             * @brief Handles keyboard input.
             * @param kn the key sequence that was pressed
             * @return true if the event was consumed
             */
            bool KeyPress(const std::string& kn) override;
            /**
             * @brief Called when the layout becomes visible or hidden.
             * @param visible true if the layout became visible
             */
            void OnVisibilityChanged(bool visible) override;
            /**
             * @brief Processes runtime messages.
             * @param message the message to process
             */
            void ProcessMessage(musik::core::runtime::IMessage &message) override;

        private:
            enum class State: int { NotPlaying, Loading, Loaded, Failed };

            void OnTrackChanged(size_t index, musik::core::TrackPtr track);
            void OnLyricsLoaded();

            void SetState(State state);
            void LoadLyricsForCurrentTrack();
            void UpdateAdapter();

            State state { State::NotPlaying };                /**< the current lyrics loading state */
            musik::core::ILibraryPtr library;                 /**< the library used to load lyrics */
            musik::core::audio::PlaybackService& playback;    /**< the playback service to follow */
            std::shared_ptr<cursespp::SimpleScrollAdapter> adapter; /**< the adapter feeding the lyrics list */
            std::shared_ptr<cursespp::ListWindow> listView;   /**< the scrollable lyrics list */
            std::shared_ptr<cursespp::TextLabel> infoText;    /**< label shown when no lyrics are available */
            cursespp::ShortcutsWindow* shortcuts;             /**< the shortcuts window, not owned */
            int64_t currentTrackId;                           /**< id of the track whose lyrics are shown */
            std::string currentLyrics;                        /**< the currently displayed lyrics text */
    };

} }