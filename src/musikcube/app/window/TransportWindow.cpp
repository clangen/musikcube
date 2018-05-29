//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include "stdafx.h"
#include "TransportWindow.h"

#include <cursespp/Screen.h>
#include <cursespp/Colors.h>
#include <cursespp/Text.h>

#include <core/support/Duration.h>

#include <core/debug.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/library/query/local/ReplayGainQuery.h>
#include <core/support/PreferenceKeys.h>
#include <core/runtime/Message.h>
#include <core/support/Playback.h>

#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <memory>
#include <deque>
#include <chrono>
#include <map>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::core::db;
using namespace musik::core::sdk;
using namespace musik::core::runtime;
using namespace musik::cube;
using namespace std::chrono;
using namespace cursespp;

#define REFRESH_INTERVAL_MS 1000
#define DEFAULT_TIME -1.0f
#define TIME_SLOP 3.0f

#define MIN_WIDTH 20
#define MIN_HEIGHT 2

#define DEBOUNCE_REFRESH(mode, delay) \
    this->DebounceMessage(message::RefreshTransport, (int64_t) mode, 0, delay);

#define ON(w, a) if (a != CURSESPP_DEFAULT_COLOR) { wattron(w, a); }
#define OFF(w, a) if (a != CURSESPP_DEFAULT_COLOR) { wattroff(w, a); }

struct Token {
    enum Type { Normal, Placeholder };

    static std::unique_ptr<Token> New(const std::string& value, Type type) {
        return std::unique_ptr<Token>(new Token(value, type));
    }

    Token(const std::string& value, Type type) {
        this->value = value;
        this->type = type;
    }

    std::string value;
    Type type;
};

typedef std::unique_ptr<Token> TokenPtr;
typedef std::vector<TokenPtr> TokenList;

/* tokenizes an input string that has $placeholder values */
void tokenize(const std::string& format, TokenList& tokens) {
    tokens.clear();
    Token::Type type = Token::Normal;
    size_t i = 0;
    size_t start = 0;
    while (i < format.size()) {
        char c = format[i];
        if ((type == Token::Placeholder && c == ' ') ||
            (type == Token::Normal && c == '$')) {
            /* escape $ with $$ */
            if (c == '$' && i < format.size() - 1 && format[i + 1] == '$') {
                i++;
            }
            else {
                if (i > start) {
                    tokens.push_back(Token::New(format.substr(start, i - start), type));
                }
                start = i;
                type = (c == ' ')  ? Token::Normal : Token::Placeholder;
            }
        }
        ++i;
    }

    if (i > 0) {
        tokens.push_back(Token::New(format.substr(start, i - start), type));
    }
}

/* a cache of localized, pre-formatted strings we use every second. */
static struct StringCache {
    std::string PLAYING_FORMAT;
    std::string STOPPED;
    std::string EMPTY_SONG;
    std::string EMPTY_ALBUM;
    std::string EMPTY_ARTIST;
    std::string SHUFFLE;
    std::string MUTED;
    std::string VOLUME;
    std::string REPEAT_LIST;
    std::string REPEAT_TRACK;
    std::string REPEAT_OFF;

    void Initialize() {
        PLAYING_FORMAT = _TSTR("transport_playing_format");
        STOPPED = _TSTR("transport_stopped");
        EMPTY_SONG = _TSTR("transport_empty_song");
        EMPTY_ALBUM = _TSTR("transport_empty_album");
        EMPTY_ARTIST = _TSTR("transport_empty_artist");
        SHUFFLE = "  " + _TSTR("transport_shuffle");
        MUTED = _TSTR("transport_muted") + "  ";
        VOLUME = _TSTR("transport_volume") + " ";
        REPEAT_LIST = "  " + _TSTR("transport_repeat_list");
        REPEAT_TRACK = "  " + _TSTR("transport_repeat_track");
        REPEAT_OFF = "  " + _TSTR("transport_repeat_off");
    }
} Strings;

/* a really boring class that contains a cache of currently playing
information so we don't have to look it up every time we update the
view (every second) */
struct musik::cube::TransportDisplayCache {
    TrackPtr track;
    std::string title, album, artist, totalTime;
    int secondsTotal;
    int titleCols, albumCols, artistCols, totalTimeCols;
    std::map<std::string, size_t> stringToColumns;

    void Reset() {
        track.reset();
        title = album = artist = "";
        titleCols = albumCols = artistCols;
        secondsTotal = 0;
        totalTime = "0:00";
        totalTimeCols = 4;
    }

    size_t Columns(const std::string& str) {
        auto it = stringToColumns.find(str);
        if (it == stringToColumns.end()) {
            stringToColumns[str] = u8cols(str);
        }
        return stringToColumns[str];
    }

    void Update(ITransport& transport, TrackPtr track) {
        if (this->track != track) {
            this->Reset();

            this->track = track;

            if (this->track) {
                title = this->track->GetString(constants::Track::TITLE);
                title = title.size() ? title : Strings.EMPTY_SONG;
                titleCols = u8cols(title);

                album = this->track->GetString(constants::Track::ALBUM);
                album = album.size() ? album : Strings.EMPTY_ALBUM;
                albumCols = u8cols(album);

                artist = this->track->GetString(constants::Track::ARTIST);
                artist = artist.size() ? artist : Strings.EMPTY_ARTIST;
                artistCols = u8cols(artist);

                secondsTotal = (int)transport.GetDuration();
                if (secondsTotal <= 0) {
                    std::string duration =
                        this->track->GetString(constants::Track::DURATION);

                    if (duration.size()) {
                        secondsTotal = boost::lexical_cast<int>(duration);
                    }
                }

                totalTime = musik::core::duration::Duration(secondsTotal);
                totalTimeCols = u8cols(totalTime);
            }
        }
    }
};

/* writes the colorized formatted string to the specified window. accounts for
utf8 characters and ellipsizing */
static size_t writePlayingFormat(
    WINDOW *w,
    TransportDisplayCache& displayCache,
    size_t width)
{
    TokenList tokens;
    tokenize(Strings.PLAYING_FORMAT, tokens);

    int64_t dim = COLOR_PAIR(CURSESPP_TEXT_DISABLED);
    int64_t gb = COLOR_PAIR(CURSESPP_TEXT_ACTIVE);
    size_t remaining = width;

    auto it = tokens.begin();
    while (it != tokens.end() && remaining > 0) {
        Token *token = it->get();

        int64_t attr = dim;
        std::string value;
        size_t cols;

        if (token->type == Token::Placeholder) {
            attr = gb;
            if (token->value == "$title") {
                value = displayCache.title;
                cols = displayCache.titleCols;
            }
            else if (token->value == "$album") {
                value = displayCache.album;
                cols = displayCache.albumCols;
            }
            else if (token->value == "$artist") {
                value = displayCache.artist;
                cols = displayCache.artistCols;
            }
        }

        if (!value.size()) {
            value = token->value;
            cols = displayCache.Columns(value);
        }

        bool ellipsized = false;

        if (cols > remaining) {
            std::string original = value;
            value = text::Ellipsize(value, remaining);
            ellipsized = (value != original);
            cols = remaining;
        }

        /* if we're not at the last token, but there's not enough space
        to show the next token, ellipsize now and bail out of the loop */
        if (remaining - cols < 3 && it + 1 != tokens.end()) {
            if (!ellipsized) {
                value = text::Ellipsize(value, remaining - 3);
                cols = remaining;
            }
        }

        ON(w, attr);
        checked_wprintw(w, value.c_str());
        OFF(w, attr);

        remaining -= cols;
        ++it;
    }

    return (width - remaining);
}

static inline bool inc(const std::string& kn) {
    return (Hotkeys::Is(Hotkeys::Right, kn));
}

static inline bool dec(const std::string& kn) {
    return (Hotkeys::Is(Hotkeys::Left, kn));
}

TransportWindow::TransportWindow(
    musik::core::ILibraryPtr library,
    musik::core::audio::PlaybackService& playback)
: Window(nullptr)
, library(library)
, replayGainMode(ReplayGainMode::Disabled)
, displayCache(new TransportDisplayCache())
, playback(playback)
, transport(playback.GetTransport())
, focus(FocusNone)
, hasReplayGain(false) {
    Strings.Initialize();
    this->SetFrameVisible(false);
    this->playback.TrackChanged.connect(this, &TransportWindow::OnPlaybackServiceTrackChanged);
    this->playback.ModeChanged.connect(this, &TransportWindow::OnPlaybackModeChanged);
    this->playback.Shuffled.connect(this, &TransportWindow::OnPlaybackShuffled);
    this->playback.VolumeChanged.connect(this, &TransportWindow::OnTransportVolumeChanged);
    this->playback.TimeChanged.connect(this, &TransportWindow::OnTransportTimeChanged);
    this->paused = false;
    this->lastTime = DEFAULT_TIME;
    this->shufflePos.y = 0;
    this->repeatPos.y = 1;
    this->volumePos.y = 1;
    this->timePos.y = 1;
    this->UpdateReplayGainState();
}

TransportWindow::~TransportWindow() {
}

void TransportWindow::SetFocus(FocusTarget target) {
    if (target != this->focus) {
        auto last = this->focus;
        this->focus = target;

        if (this->focus == FocusNone) {
            this->lastFocus = last;
            this->Blur();
        }
        else {
            this->Focus();
        }

        DEBOUNCE_REFRESH(TimeSync, 0);
    }
}

TransportWindow::FocusTarget TransportWindow::GetFocus() const {
    return this->focus;
}

bool TransportWindow::KeyPress(const std::string& kn) {
    if (this->focus == FocusVolume) {
        if (inc(kn)) {
            playback::VolumeUp(this->transport);
            return true;
        }
        else if (dec(kn)) {
            playback::VolumeDown(this->transport);
            return true;
        }
        else if (kn == "KEY_ENTER") {
            transport.SetMuted(!transport.IsMuted());
            return true;
        }
    }
    else if (this->focus == FocusTime) {
        if (inc(kn)) {
            playback::SeekForward(this->playback);
            return true;
        }
        else if (dec(kn)) {
            playback::SeekBack(this->playback);
            return true;
        }
    }

    return false;
}

bool TransportWindow::MouseEvent(const IMouseHandler::Event& event) {
    if (event.Button1Clicked()) {
        if (this->shufflePos.Contains(event)) {
            this->playback.ToggleShuffle();
            return true;
        }
        else if (this->repeatPos.Contains(event)) {
            this->playback.ToggleRepeatMode();
            return true;
        }
        else if (this->volumePos.Contains(event)) {
            if (playback.IsMuted()) {
                playback.ToggleMute();
            }
            else {
                playback.SetVolume(this->volumePos.Percent(event.x));
            }
            return true;
        }
        else if (this->timePos.Contains(event)) {
            if (playback.GetPlaybackState() != PlaybackStopped) {
                double duration = playback.GetDuration();
                double percent = this->timePos.Percent(event.x);
                playback.SetPosition(duration * percent);
            }
            return true;
        }
    }
    else if (event.Button3Clicked()) {
        if (this->volumePos.Contains(event)) {
            if (!playback.IsMuted()) {
                playback.ToggleMute();
            }
        }
    }
    return false;
}

bool TransportWindow::FocusNext() {
    this->SetFocus((FocusTarget)(((int) this->focus + 1) % 3));
    return (this->focus != FocusNone);
}

bool TransportWindow::FocusPrev() {
    this->SetFocus((FocusTarget)(((int) this->focus - 1) % 3));
    return (this->focus != FocusNone);
}

void TransportWindow::FocusFirst() {
    this->SetFocus(FocusVolume);
}

void TransportWindow::FocusLast() {
    this->SetFocus(FocusTime);
}

void TransportWindow::RestoreFocus() {
    this->Focus();
    this->SetFocus(this->lastFocus);
}

void TransportWindow::OnFocusChanged(bool focused) {
    if (!focused) {
        this->SetFocus(FocusNone);
    }
}

void TransportWindow::ProcessMessage(IMessage &message) {
    int type = message.Type();

    if (type == message::RefreshTransport) {
        this->Update((TimeMode) message.UserData1());

        if (transport.GetPlaybackState() != PlaybackStopped) {
            DEBOUNCE_REFRESH(TimeSmooth, REFRESH_INTERVAL_MS)
        }
    }
}

void TransportWindow::OnPlaybackServiceTrackChanged(size_t index, TrackPtr track) {
    this->currentTrack = track;
    this->lastTime = DEFAULT_TIME;
    this->UpdateReplayGainState();
    DEBOUNCE_REFRESH(TimeSync, 0);
}

void TransportWindow::OnPlaybackModeChanged() {
    DEBOUNCE_REFRESH(TimeSync, 0);
}

void TransportWindow::OnTransportVolumeChanged() {
    DEBOUNCE_REFRESH(TimeSync, 0);
}

void TransportWindow::OnTransportTimeChanged(double time) {
    DEBOUNCE_REFRESH(TimeSync, 0);
}

void TransportWindow::OnPlaybackShuffled(bool shuffled) {
    DEBOUNCE_REFRESH(TimeSync, 0);
}

void TransportWindow::OnRedraw() {
    this->Update();
}

void TransportWindow::UpdateReplayGainState() {
    using Mode = ReplayGainMode;
    using Query = db::local::ReplayGainQuery;

    auto prefs = Preferences::ForComponent(prefs::components::Playback);

    this->replayGainMode = (Mode)
        prefs->GetInt(prefs::keys::ReplayGainMode.c_str(), (int) Mode::Disabled);

    this->hasReplayGain = false;

    if (this->replayGainMode != Mode::Disabled) {
        if (this->currentTrack) {
            auto query = std::make_shared<Query>(this->currentTrack->GetId());
            if (this->library->Enqueue(query, ILibrary::QuerySynchronous)) {
                auto result = query->GetResult();

                this->hasReplayGain = result && (
                    result->albumGain != 1.0f || result->albumPeak != 1.0f ||
                    result->trackGain != 1.0f || result->albumPeak != 1.0f);
            }
        }
    }
}

void TransportWindow::Update(TimeMode timeMode) {
    this->Clear();

    size_t cx = (size_t) this->GetContentWidth();

    if (cx < MIN_WIDTH || this->GetContentHeight() < MIN_HEIGHT) {
        return;
    }

    WINDOW *c = this->GetContent();
    auto state = transport.GetPlaybackState();
    bool paused = (state == PlaybackPrepared || state == PlaybackPaused);
    bool stopped = (state == PlaybackStopped);
    bool muted = transport.IsMuted();
    bool replayGainEnabled = (this->replayGainMode != ReplayGainMode::Disabled);

    int64_t gb = COLOR_PAIR(CURSESPP_TEXT_ACTIVE);
    int64_t disabled = COLOR_PAIR(CURSESPP_TEXT_DISABLED);
    int64_t bright = COLOR_PAIR(CURSESPP_TEXT_DEFAULT);

    int64_t volumeAttrs = CURSESPP_DEFAULT_COLOR;
    if (this->focus == FocusVolume) {
        volumeAttrs = COLOR_PAIR(CURSESPP_TEXT_FOCUSED);
    }
    else if (muted) {
        volumeAttrs = gb;
    }

    int64_t timerAttrs = (this->focus == FocusTime)
        ? COLOR_PAIR(CURSESPP_TEXT_FOCUSED) : CURSESPP_DEFAULT_COLOR;

    /* prepare the "shuffle" label */

    std::string shuffleLabel = Strings.SHUFFLE;
    size_t shuffleWidth = displayCache->Columns(shuffleLabel);

    /* playing SONG TITLE from ALBUM NAME */

    if (stopped) {
        ON(c, disabled);
        checked_wprintw(c, Strings.STOPPED.c_str());
        displayCache->Reset();
        OFF(c, disabled);
    }
    else {
        displayCache->Update(transport, this->currentTrack);
        writePlayingFormat(c, *this->displayCache, cx - shuffleWidth);
    }

    /* draw the "shuffle" label */
    const int shuffleOffset = cx - shuffleWidth;
    wmove(c, 0, shuffleOffset);
    int64_t shuffleAttrs = this->playback.IsShuffled() ? gb : disabled;
    ON(c, shuffleAttrs);
    checked_wprintw(c, shuffleLabel.c_str());
    OFF(c, shuffleAttrs);
    this->shufflePos.Set(shuffleOffset, shuffleWidth);

    /* volume slider */

    int volumePercent = (size_t) round(this->transport.Volume() * 100.0f);
    int thumbOffset = std::min(10, (volumePercent * 10) / 100);

    std::string volume;

    if (muted) {
        volume = Strings.MUTED;
        this->volumePos.Set(0, u8cols(Strings.MUTED));
    }
    else {
        volume = Strings.VOLUME;
        this->volumePos.Set(u8cols(Strings.VOLUME), 11);

        for (int i = 0; i < 11; i++) {
            volume += (i == thumbOffset) ? "■" : "─";
        }

        volume += boost::str(boost::format(
            " %d") % (int)std::round(this->transport.Volume() * 100));

        volume += replayGainEnabled ? "%% " : "%%  ";
    }

    /* repeat mode setup */

    RepeatMode mode = this->playback.GetRepeatMode();
    std::string repeatModeLabel;
    int64_t repeatAttrs = CURSESPP_DEFAULT_COLOR;
    switch (mode) {
        case RepeatList:
            repeatModeLabel += Strings.REPEAT_LIST;
            repeatAttrs = gb;
            break;
        case RepeatTrack:
            repeatModeLabel += Strings.REPEAT_TRACK;
            repeatAttrs = gb;
            break;
        default:
            repeatModeLabel += Strings.REPEAT_OFF;
            repeatAttrs = disabled;
            break;
    }

    /* time slider */

    int64_t currentTimeAttrs = timerAttrs;

    if (paused) { /* blink the track if paused */
        int64_t now = duration_cast<seconds>(
            system_clock::now().time_since_epoch()).count();

        if (now % 2 == 0) {
            currentTimeAttrs = COLOR_PAIR(CURSESPP_TEXT_HIDDEN);
        }
    }

    /* calculating playback time is inexact because it's based on buffers that
    are sent to the output. here we use a simple smoothing function to hopefully
    mitigate jumping around. basically: draw the time as one second more than the
    last time we displayed, unless they are more than few seconds apart. note this
    only works if REFRESH_INTERVAL_MS is 1000. */
    int secondsCurrent = (int) round(this->lastTime); /* mode == TimeLast */

    if (timeMode == TimeSmooth) {
        double smoothedTime = this->lastTime += 1.0f; /* 1000 millis */
        double actualTime = playback.GetPosition();

        if (paused || stopped || fabs(smoothedTime - actualTime) > TIME_SLOP) {
            smoothedTime = actualTime;
        }

        this->lastTime = smoothedTime;
        /* end time smoothing */

        secondsCurrent = (int) round(smoothedTime);
    }
    else if (timeMode == TimeSync) {
        this->lastTime = playback.GetPosition();
        secondsCurrent = (int) round(this->lastTime);
    }

    const std::string currentTime = musik::core::duration::Duration(
        std::min(secondsCurrent, displayCache->secondsTotal));

    const std::string replayGain = replayGainEnabled  ? "rg" : "";

    int bottomRowControlsWidth =
        displayCache->Columns(volume) - (muted ? 0 : 1) + /* -1 for escaped percent sign when not muted */
        (replayGainEnabled ? (u8cols(replayGain) + 4) : 0) +  /* [] brackets */
        u8cols(currentTime) + 1 + /* +1 for space padding */
        /* timer track with thumb */
        1 + displayCache->totalTimeCols + /* +1 for space padding */
        displayCache->Columns(repeatModeLabel);

    int timerTrackWidth =
        this->GetContentWidth() -
        bottomRowControlsWidth;

    thumbOffset = 0;

    if (displayCache->secondsTotal) {
        int progress = (secondsCurrent * 100) / displayCache->secondsTotal;
        thumbOffset = std::min(timerTrackWidth - 1, (progress * timerTrackWidth) / 100);
    }

    std::string timerTrack = "";
    for (int i = 0; i < timerTrackWidth; i++) {
        timerTrack += (i == thumbOffset) ? "■" : "─";
    }

    /* draw second row */

    wmove(c, 1, 0); /* move cursor to the second line */

    ON(c, volumeAttrs);
    checked_wprintw(c, volume.c_str());
    OFF(c, volumeAttrs);

    if (replayGainEnabled) {
        auto rgStyle = this->hasReplayGain ? gb : disabled;
        checked_wprintw(c, "[");
        ON(c, rgStyle); checked_wprintw(c, "%s", replayGain.c_str()); OFF(c, rgStyle);
        checked_wprintw(c, "]  ");
    }

    ON(c, currentTimeAttrs); /* blink if paused */
    checked_wprintw(c, "%s ", currentTime.c_str());
    OFF(c, currentTimeAttrs);

    ON(c, timerAttrs);
    this->timePos.Set(getcurx(c), u8cols(timerTrack));
    checked_waddstr(c, timerTrack.c_str()); /* may be a very long string */
    checked_wprintw(c, " %s", displayCache->totalTime.c_str());
    OFF(c, timerAttrs);

    ON(c, repeatAttrs);
    this->repeatPos.Set(getcurx(c), u8cols(repeatModeLabel));
    checked_wprintw(c, repeatModeLabel.c_str());
    OFF(c, repeatAttrs);

    this->Invalidate();
}