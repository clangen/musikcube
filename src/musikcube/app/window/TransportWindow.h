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

        class TransportWindow:
            public cursespp::Window,
            public cursespp::IKeyHandler,
            public sigslot::has_slots<>
        {
            public:
                enum FocusTarget {
                    FocusNone = 0,
                    FocusVolume = 1,
                    FocusTime = 2
                };

                TransportWindow(
                    musik::core::ILibraryPtr library,
                    musik::core::audio::PlaybackService& playback);

                virtual ~TransportWindow();

                void ProcessMessage(musik::core::runtime::IMessage &message) override;
                void OnFocusChanged(bool focused) override;
                void OnRedraw() override;
                bool KeyPress(const std::string& key) override;
                bool ProcessMouseEvent(const IMouseHandler::Event& mouseEvent) override;

                void SetFocus(FocusTarget target);
                FocusTarget GetFocus() const;
                bool FocusNext();
                bool FocusPrev();
                void FocusFirst();
                void FocusLast();
                void RestoreFocus();

            private:
                enum class TimeMode: int {
                    Last = 0,
                    Smooth = 1,
                    Sync = 2
                };

                /* a little structure used to make mouse event handling a bit
                less verbose. */
                struct Position {
                    Position() noexcept;
                    Position(int x, int y, int width) noexcept;
                    void Set(int x, int width) noexcept;
                    void Set(int x, int y, int width) noexcept;
                    double Percent(int x) noexcept;
                    bool Contains(const IMouseHandler::Event& event) noexcept;
                    int x, y, width;
                };

                /* the transport updates at least once a second, and displays computed
                values. this cache just holds those computed values so they don't
                need to be recalculated constantly. */
                struct DisplayCache {
                    void Reset();
                    size_t Columns(const std::string& str);
                    std::string CurrentTime(int secondsCurrent);
                    void Update(musik::core::audio::ITransport& transport, musik::core::TrackPtr track);

                    musik::core::TrackPtr track;
                    std::string title, album, artist, totalTime;
                    int secondsTotal;
                    int titleCols, albumCols, artistCols, totalTimeCols;
                    std::map<std::string, size_t> stringToColumns;
                };

                size_t WritePlayingFormat(WINDOW* w, size_t width);

                void Update(TimeMode mode = TimeMode::Smooth);

                void OnPlaybackServiceTrackChanged(size_t index, musik::core::TrackPtr track);
                void OnPlaybackModeChanged();
                void OnPlaybackStateChanged(musik::core::sdk::PlaybackState);
                void OnPlaybackStreamStateChanged(musik::core::sdk::StreamState);
                void OnTransportVolumeChanged();
                void OnTransportTimeChanged(double time);
                void OnPlaybackShuffled(bool shuffled);
                void UpdateReplayGainState();

                bool paused;
                bool hasReplayGain;
                Position shufflePos, repeatPos, volumePos, currentTimePos, timeBarPos;
                std::map<std::string, Position> metadataFieldToPosition;
                musik::core::sdk::ReplayGainMode replayGainMode;
                musik::core::ILibraryPtr library;
                musik::core::audio::ITransport& transport;
                musik::core::audio::PlaybackService& playback;
                musik::core::TrackPtr currentTrack;
                FocusTarget focus, lastFocus;
                DisplayCache displayCache;
                bool buffering{ false };
                double lastTime;
        };
    }
}
