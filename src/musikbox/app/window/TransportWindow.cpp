//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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
#include <cursespp/Message.h>
#include <cursespp/Text.h>

#include <app/util/Duration.h>

#include <core/debug.h>
#include <core/library/LocalLibraryConstants.h>

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/chrono.hpp>
#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <memory>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::core::db;
using namespace musik::box;
using namespace boost::chrono;
using namespace cursespp;

#define REFRESH_TRANSPORT_READOUT 1001
#define REFRESH_INTERVAL_MS 1000
#define DEFAULT_TIME -1.0f
#define TIME_SLOP 3.0f

#define MIN_WIDTH 20
#define MIN_HEIGHT 2

#define DEBOUNCE_REFRESH(x) \
    this->RemoveMessage(REFRESH_TRANSPORT_READOUT); \
    this->PostMessage(REFRESH_TRANSPORT_READOUT, 0, 0, x);

#define DEBOUNCE_REFRESH_AND_SYNC_TIME() \
    this->lastTime = this->transport.Position(); \
    DEBOUNCE_REFRESH(0)


#define ON(w, a) if (a != -1) { wattron(w, a); }
#define OFF(w, a) if (a != -1) { wattroff(w, a); }

static std::string playingFormat = "playing $title by $artist from $album";

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

/* writes the colorized formatted string to the specified window. accounts for
utf8 characters and ellipsizing */
size_t writePlayingFormat(
    WINDOW *w,
    std::string title,
    std::string album,
    std::string artist,
    size_t width)
{
    TokenList tokens;
    tokenize(playingFormat, tokens);

    int64 dim = COLOR_PAIR(CURSESPP_TEXT_DISABLED);
    int64 gb = COLOR_PAIR(CURSESPP_TEXT_ACTIVE);
    size_t remaining = width;

    auto it = tokens.begin();
    while (it != tokens.end() && remaining > 0) {
        Token *token = it->get();

        int64 attr = dim;
        std::string value;

        if (token->type == Token::Placeholder) {
            attr = gb;
            if (token->value == "$title") {
                value = title;
            }
            else if (token->value == "$album") {
                value = album;
            }
            else if (token->value == "$artist") {
                value = artist;
            }
        }

        if (!value.size()) {
            value = token->value;
        }

        size_t len = u8cols(value);
        bool ellipsized = false;

        if (len > remaining) {
            std::string original = value;
            value = text::Ellipsize(value, remaining);
            ellipsized = (value != original);
            len = remaining;
        }

        /* if we're not at the last token, but there's not enough space
        to show the next token, ellipsize now and bail out of the loop */        
        if (remaining - len < 3) {
            if (!ellipsized) {
                value = text::Ellipsize(value, remaining - 3);
                len = remaining;
            }
        }

        ON(w, attr);
        wprintw(w, value.c_str());
        OFF(w, attr);

        remaining -= len;
        ++it;
    }

    return (width - remaining);
}

TransportWindow::TransportWindow(musik::box::PlaybackService& playback)
: Window(NULL)
, playback(playback)
, transport(playback.GetTransport())
{
    this->SetFrameVisible(false);
    this->playback.TrackChanged.connect(this, &TransportWindow::OnPlaybackServiceTrackChanged);
    this->playback.ModeChanged.connect(this, &TransportWindow::OnPlaybackModeChanged);
    this->playback.Shuffled.connect(this, &TransportWindow::OnPlaybackShuffled);
    this->transport.VolumeChanged.connect(this, &TransportWindow::OnTransportVolumeChanged);
    this->transport.TimeChanged.connect(this, &TransportWindow::OnTransportTimeChanged);
    this->paused = false;
    this->lastTime = DEFAULT_TIME;
}

TransportWindow::~TransportWindow() {
}

void TransportWindow::Show() {
    Window::Show();
    this->Update();
}

void TransportWindow::ProcessMessage(IMessage &message) {
    int type = message.Type();

    if (type == REFRESH_TRANSPORT_READOUT) {
        this->Update();
        DEBOUNCE_REFRESH(REFRESH_INTERVAL_MS)
    }
}

void TransportWindow::OnPlaybackServiceTrackChanged(size_t index, TrackPtr track) {
    this->currentTrack = track;
    this->lastTime = DEFAULT_TIME;
    DEBOUNCE_REFRESH(0);
}

void TransportWindow::OnPlaybackModeChanged() {
    DEBOUNCE_REFRESH_AND_SYNC_TIME();
}

void TransportWindow::OnTransportVolumeChanged() {
    DEBOUNCE_REFRESH_AND_SYNC_TIME();
}

void TransportWindow::OnTransportTimeChanged(double time) {
    DEBOUNCE_REFRESH_AND_SYNC_TIME();
}

void TransportWindow::OnPlaybackShuffled(bool shuffled) {
    DEBOUNCE_REFRESH_AND_SYNC_TIME();
}

void TransportWindow::Update() {
    this->Clear();

    size_t cx = (size_t) this->GetContentWidth();

    if (cx < MIN_WIDTH || this->GetContentHeight() < MIN_HEIGHT) {
        return;
    }

    WINDOW *c = this->GetContent();
    bool paused = (transport.GetPlaybackState() == ITransport::PlaybackPaused);
    bool stopped = (transport.GetPlaybackState() == ITransport::PlaybackStopped);

    int64 gb = COLOR_PAIR(CURSESPP_TEXT_ACTIVE);
    int64 disabled = COLOR_PAIR(CURSESPP_TEXT_DISABLED);

    /* prepare the "shuffle" label */

    std::string shuffleLabel = " shuffle";
    size_t shuffleLabelLen = u8cols(shuffleLabel);

    /* playing SONG TITLE from ALBUM NAME */

    std::string duration = "0";

    if (stopped) {
        ON(c, disabled);
        wprintw(c, "playback is stopped");
        OFF(c, disabled);
    }
    else {
        std::string title, album, artist;

        if (this->currentTrack) {
            title = this->currentTrack->GetValue(constants::Track::TITLE);
            album = this->currentTrack->GetValue(constants::Track::ALBUM);
            artist = this->currentTrack->GetValue(constants::Track::ARTIST);
            duration = this->currentTrack->GetValue(constants::Track::DURATION);
        }

        title = title.size() ? title : "[song]";
        album = album.size() ? album : "[album]";
        artist = artist.size() ? artist : "[artist]";
        duration = duration.size() ? duration : "0";

        writePlayingFormat(
            c,
            title,
            album,
            artist,
            cx - shuffleLabelLen);
    }

    wmove(c, 0, cx - shuffleLabelLen);
    int64 shuffleAttrs = this->playback.IsShuffled() ? gb : disabled;
    ON(c, shuffleAttrs);
    wprintw(c, shuffleLabel.c_str());
    OFF(c, shuffleAttrs);

    wmove(c, 1, 0); /* move cursor to the second line */

    /* volume slider */

    int volumePercent = (size_t) round(this->transport.Volume() * 100.0f) - 1;
    int thumbOffset = std::min(9, (volumePercent * 10) / 100);

    std::string volume = "vol ";

    for (int i = 0; i < 10; i++) {
        volume += (i == thumbOffset) ? "■" : "─";
    }

    volume += "  ";

    wprintw(c, volume.c_str());

    /* repeat mode setup */

    PlaybackService::RepeatMode mode = this->playback.GetRepeatMode();
    std::string repeatLabel = " ∞ ";
    std::string repeatModeLabel;
    int64 repeatAttrs = -1;
    switch (mode) {
        case PlaybackService::RepeatList:
            repeatModeLabel = "list";
            repeatAttrs = gb;
            break;
        case PlaybackService::RepeatTrack:
            repeatModeLabel = "track";
            repeatAttrs = gb;
            break;
        default:
            repeatModeLabel = "off";
            repeatAttrs = disabled;
            break;
    }

    /* time slider */

    int64 timerAttrs = 0;

    if (paused) { /* blink the track if paused */
        int64 now = duration_cast<seconds>(
            system_clock::now().time_since_epoch()).count();

        if (now % 2 == 0) {
            timerAttrs = COLOR_PAIR(CURSESPP_TEXT_HIDDEN);
        }
    }

    transport.Position();

    /* calculating playback time is inexact because it's based on buffers that
    are sent to the output. here we use a simple smoothing function to hopefully
    mitigate jumping around. basically: draw the time as one second more than the
    last time we displayed, unless they are more than few seconds apart. note this
    only works if REFRESH_INTERVAL_MS is 1000. */
    double smoothedTime = this->lastTime += 1.0f; /* 1000 millis */
    double actualTime = transport.Position();

    if (paused || stopped || fabs(smoothedTime - actualTime) > TIME_SLOP) {
        smoothedTime = actualTime;
    }

    this->lastTime = smoothedTime;
    /* end time smoothing */

    int secondsCurrent = (int) round(smoothedTime);
    int secondsTotal = boost::lexical_cast<int>(duration);

    std::string currentTime = duration::Duration(std::min(secondsCurrent, secondsTotal));
    std::string totalTime = duration::Duration(secondsTotal);

    int timerWidth =
        this->GetContentWidth() -
        u8cols(volume) -
        (u8cols(repeatLabel) + u8cols(repeatModeLabel)) -
        currentTime.size() -
        totalTime.size() -
        2; /* padding */

    thumbOffset = 0;

    if (secondsTotal) {
        int progress = (secondsCurrent * 100) / secondsTotal;
        thumbOffset = std::min(timerWidth - 1, (progress * timerWidth) / 100);
    }

    std::string timerTrack = "";
    for (int i = 0; i < timerWidth; i++) {
        timerTrack += (i == thumbOffset) ? "■" : "─";
    }

    wattron(c, timerAttrs); /* blink if paused */
    wprintw(c, currentTime.c_str());
    wattroff(c, timerAttrs);

    /* using wprintw() here on large displays (1440p+) will exceed the internal
    buffer length of 512 characters, so use boost format. */
    std::string fmt = boost::str(boost::format(
        " %s %s") % timerTrack % totalTime);

    waddstr(c, fmt.c_str());

    /* repeat mode draw */
    wprintw(c, repeatLabel.c_str());
    wattron(c, repeatAttrs);
    wprintw(c, repeatModeLabel.c_str());
    wattroff(c, repeatAttrs);

    this->Repaint();
}
