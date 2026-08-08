//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

/**
 * @file TransportWindow.h
 * @brief Window that renders the playback transport and track metadata.
 * @details Shows the current track, its album and artist, the elapsed and
 *          total time, and interactive controls for shuffle, repeat, volume
 *          and the time bar. Supports keyboard focus cycling and mouse
 *          interaction on the individual controls.
 */

#include <cursespp/Window.h>
#include <cursespp/IKeyHandler.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/runtime/IMessage.h>
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/support/PreferenceKeys.h>
#include <sigslot/sigslot.h>

namespace musik {
    namespace cube {
        struct TransportDisplayCache;

        /**
         * @brief The main playback transport bar.
         * @details Updates roughly once per second from the transport time,
         *          displays the metadata of the current track and renders
         *          interactive shuffle, repeat, volume and time controls. The
         *          left and right keys and the mouse move between the focus
         *          targets.
         */
        class TransportWindow:
            public cursespp::Window,
            public cursespp::IKeyHandler,
            public sigslot::has_slots<>
        {
            public:
                /**
                 * @brief The keyboard-focusable regions of the window.
                 */
                enum FocusTarget {
                    FocusNone = 0,   /**< no region is focused */
                    FocusVolume = 1, /**< the volume control */
                    FocusTime = 2    /**< the time bar */
                };

                /**
                 * @brief Creates the transport window bound to the given
                 *        library and playback service.
                 * @param library the library providing track metadata
                 * @param playback the playback service being rendered
                 */
                TransportWindow(
                    musik::core::ILibraryPtr library,
                    musik::core::audio::PlaybackService& playback);

                /**
                 * @brief Destroys the window.
                 */
                virtual ~TransportWindow();

                /**
                 * @brief Processes runtime messages.
                 * @param message the message to process
                 */
                void ProcessMessage(musik::core::runtime::IMessage &message) override;
                /**
                 * @brief Called when the window focus changes.
                 * @param focused true if the window gained focus
                 */
                void OnFocusChanged(bool focused) override;
                /**
                 * @brief Renders the window contents.
                 */
                void OnRedraw() override;
                /**
                 * @brief Handles keyboard input.
                 * @param key the key sequence that was pressed
                 * @return true if the event was consumed
                 */
                bool KeyPress(const std::string& key) override;
                /**
                 * @brief Handles mouse events over the controls.
                 * @param mouseEvent the mouse event
                 * @return true if the event was consumed
                 */
                bool ProcessMouseEvent(const IMouseHandler::Event& mouseEvent) override;

                /**
                 * @brief Sets the focused region.
                 * @param target the region to focus
                 */
                void SetFocus(FocusTarget target);
                /**
                 * @brief Returns the currently focused region.
                 * @return the focus target
                 */
                FocusTarget GetFocus() const;
                /**
                 * @brief Moves focus to the next region.
                 * @return true if focus moved
                 */
                bool FocusNext();
                /**
                 * @brief Moves focus to the previous region.
                 * @return true if focus moved
                 */
                bool FocusPrev();
                /**
                 * @brief Focuses the first region.
                 */
                void FocusFirst();
                /**
                 * @brief Focuses the last region.
                 */
                void FocusLast();
                /**
                 * @brief Restores the last remembered focus target.
                 */
                void RestoreFocus();

            private:
                /**
                 * @brief How the time values are refreshed.
                 */
                enum class TimeMode: int {
                    Last = 0,    /**< keep the last computed values */
                    Smooth = 1,  /**< recompute from the transport smoothly */
                    Sync = 2     /**< recompute and sync with the track */
                };

                /* a little structure used to make mouse event handling a bit
                less verbose. */
                /**
                 * @brief A rectangular region of the transport window.
                 * @details Used to hit-test mouse events against the shuffle,
                 *          repeat, volume and time controls.
                 */
                struct Position {
                    /**
                     * @brief Creates an empty position.
                     */
                    Position() noexcept;
                    /**
                     * @brief Creates a position with the given extents.
                     * @param x the left edge
                     * @param y the top edge
                     * @param width the width in columns
                     */
                    Position(int x, int y, int width) noexcept;
                    /**
                     * @brief Sets the left edge and width.
                     * @param x the left edge
                     * @param width the width in columns
                     */
                    void Set(int x, int width) noexcept;
                    /**
                     * @brief Sets the left edge, top edge and width.
                     * @param x the left edge
                     * @param y the top edge
                     * @param width the width in columns
                     */
                    void Set(int x, int y, int width) noexcept;
                    /**
                     * @brief Converts an x coordinate to a percentage.
                     * @param x the column position
                     * @return the position as a value from 0 to 1
                     */
                    double Percent(int x) noexcept;
                    /**
                     * @brief Returns true if the mouse event falls inside.
                     * @param event the mouse event
                     * @return true if the position contains the event
                     */
                    bool Contains(const IMouseHandler::Event& event) noexcept;
                    int x, y, width; /**< the region extents */
                };

                /* the transport updates at least once a second, and displays computed
                values. this cache just holds those computed values so they don't
                need to be recalculated constantly. */
                /**
                 * @brief Cached values computed from the transport state.
                 * @details Avoids recomputing formatted strings and column
                 *          widths on every redraw.
                 */
                struct DisplayCache {
                    /**
                     * @brief Clears all cached values.
                     */
                    void Reset();
                    /**
                     * @brief Returns the display width of a string.
                     * @param str the string to measure
                     * @return the width in columns
                     */
                    size_t Columns(const std::string& str);
                    /**
                     * @brief Formats the elapsed time.
                     * @param secondsCurrent the elapsed seconds
                     * @return the formatted time string
                     */
                    std::string CurrentTime(int secondsCurrent);
                    /**
                     * @brief Recomputes the cached values from the transport.
                     * @param transport the transport to read from
                     * @param track the current track
                     */
                    void Update(musik::core::audio::ITransport& transport, musik::core::TrackPtr track);

                    musik::core::TrackPtr track;            /**< the cached track */
                    std::string title, album, artist, totalTime; /**< cached display strings */
                    int secondsTotal;                       /**< the cached total seconds */
                    int titleCols, albumCols, artistCols, totalTimeCols; /**< cached column widths */
                    std::map<std::string, size_t> stringToColumns; /**< cached string widths */
                };

                /**
                 * @brief Writes the now playing line to the window.
                 * @param w the window to draw into
                 * @param width the available width
                 * @return the number of columns written
                 */
                size_t WritePlayingFormat(WINDOW* w, size_t width);

                /**
                 * @brief Recomputes and redraws the transport.
                 * @param mode the refresh mode
                 */
                void Update(TimeMode mode = TimeMode::Smooth);

                /**
                 * @brief Handles the playback track changing.
                 * @param index the index of the new track
                 * @param track the new track
                 */
                void OnPlaybackServiceTrackChanged(size_t index, musik::core::TrackPtr track);
                /**
                 * @brief Handles the playback mode changing.
                 */
                void OnPlaybackModeChanged();
                /**
                 * @brief Handles the playback state changing.
                 * @param state the new playback state
                 */
                void OnPlaybackStateChanged(musik::core::sdk::PlaybackState);
                /**
                 * @brief Handles the stream state changing.
                 * @param state the new stream state
                 */
                void OnPlaybackStreamStateChanged(musik::core::sdk::StreamState);
                /**
                 * @brief Handles the volume changing.
                 */
                void OnTransportVolumeChanged();
                /**
                 * @brief Handles the transport time changing.
                 * @param time the new time in seconds
                 */
                void OnTransportTimeChanged(double time);
                /**
                 * @brief Handles the queue being shuffled.
                 * @param shuffled true if the queue is now shuffled
                 */
                void OnPlaybackShuffled(bool shuffled);
                /**
                 * @brief Refreshes the cached replay gain state.
                 */
                void UpdateReplayGainState();

                bool paused;                                    /**< true while playback is paused */
                bool hasReplayGain;                             /**< true if the track has replay gain */
                Position shufflePos, repeatPos, volumePos, currentTimePos, timeBarPos; /**< interactive regions */
                std::map<std::string, Position> metadataFieldToPosition; /**< metadata region positions */
                musik::core::sdk::ReplayGainMode replayGainMode; /**< the active replay gain mode */
                musik::core::ILibraryPtr library;              /**< the library providing metadata */
                musik::core::audio::ITransport& transport;      /**< the transport being rendered */
                musik::core::audio::PlaybackService& playback;  /**< the playback service */
                musik::core::TrackPtr currentTrack;             /**< the currently displayed track */
                FocusTarget focus, lastFocus;                   /**< current and previous focus targets */
                DisplayCache displayCache;                      /**< cached display values */
                bool buffering{ false };                        /**< true while the stream is buffering */
                double lastTime;                                /**< the last displayed time */
        };
    }
}
