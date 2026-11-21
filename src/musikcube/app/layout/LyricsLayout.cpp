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

#include <stdafx.h>
#include <app/layout/LyricsLayout.h>
#include <app/util/Messages.h>
#include <musikcore/i18n/Locale.h>
#include <musikcore/support/Auddio.h>
#include <musikcore/sdk/String.h>
#include <cursespp/App.h>
#include <cursespp/Screen.h>
#include <cursespp/ToastOverlay.h>
#include <cursespp/SingleLineEntry.h>
#include <app/util/Hotkeys.h>
#include <musikcore/runtime/Message.h>
#include <app/util/Messages.h>

using namespace musik::cube;
using namespace musik::core;
using namespace musik::core::runtime;
using namespace musik::core::sdk;
using namespace cursespp;

LyricsLayout::LyricsLayout(api::Context& musikcore)
: LayoutBase()
, currentTrackId(-1LL)
, musikcore(musikcore)
, playback(musikcore.GetPlayback()) {

    this->adapter = std::make_shared<SimpleScrollAdapter>();
    this->adapter->SetSelectable(true);

    this->listView = std::make_shared<ListWindow>(this->adapter);
    this->AddWindow(this->listView);
    this->listView->SetFocusOrder(0);

    this->infoText = std::make_shared<TextLabel>("", text::AlignCenter);
    this->AddWindow(this->infoText);

    this->playback.SetChangedCallback([](void* data, mc_playback_event) {
        auto& view = *static_cast<LyricsLayout*>(data);
        view.currentTrackId = -1;
        if (view.IsVisible()) {
            view.LoadLyricsForCurrentTrack();
        }
        else {
            view.lyricsWork.Invalidate();
        }
    }, this, { MC_PLAYBACK_CHANGED_TRACK });
}

void LyricsLayout::OnLayout() {
    LayoutBase::OnLayout();
    const int cx = this->GetContentWidth();
    const int cy = this->GetContentHeight();
    this->listView->MoveAndResize(0, 0, cx, cy);
    this->infoText->MoveAndResize(1, cy / 2, cx - 2, 1);
}

void LyricsLayout::OnLyricsLoaded(std::string lyrics) {
    if (lyrics.empty()) {
        this->SetState(State::Failed);
        return;
    }

    this->currentLyrics = std::move(lyrics);
    this->UpdateAdapter();
    this->listView->ScrollTo(0);
    this->listView->SetSelectedIndex(0);
    this->listView->SetFrameTitle(u8fmt(
        _TSTR("lyrics_list_title"), this->trackTitle.c_str(), this->trackArtist.c_str()));
    this->SetState(State::Loaded);
}

bool LyricsLayout::KeyPress(const std::string& kn) {
    if (Hotkeys::Is(Hotkeys::LyricsRetry, kn)) {
        this->currentTrackId = -1;
        this->LoadLyricsForCurrentTrack();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryPlayQueue, kn)) {
        this->Broadcast(message::JumpToPlayQueue);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibrary, kn)) {
        this->Broadcast(message::JumpToLibrary);
        return true;
    }
    else if (kn == " ") { /* ugh... need to generalize this maybe */
        this->playback.Control(MC_PLAYBACK_TOGGLE_PAUSE);
        return true;
    }

    return LayoutBase::KeyPress(kn);
}

void LyricsLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);
    if (visible) {
        this->LoadLyricsForCurrentTrack();
        this->FocusFirst();
    }
}

namespace {
    class LyricsLoadedMessage final : public Message {
    public:
        LyricsLoadedMessage(IMessageTarget* target, AsyncCallbackScope::Token token, std::string lyrics)
            : Message(target, musik::cube::message::LyricsLoaded, 0, 0), token(token), lyrics(std::move(lyrics)) {}

        AsyncCallbackScope::Token token;
        std::string lyrics;
    };
}

void LyricsLayout::ProcessMessage(IMessage& message) {
    if (message.Type() == message::LyricsLoaded) {
        auto& result = static_cast<LyricsLoadedMessage&>(message);
        if (result.token.IsCurrent()) {
            this->OnLyricsLoaded(std::move(result.lyrics));
        }
    }
    else {
        LayoutBase::ProcessMessage(message);
    }
}

void LyricsLayout::LoadLyricsForCurrentTrack() {
    try {
        const auto track = this->playback.CurrentTrack();
        if (!track) {
            this->lyricsWork.Invalidate();
            this->SetState(State::NotPlaying);
            return;
        }
        if (track->Id() == this->currentTrackId) {
            return;
        }

        this->currentTrackId = track->Id();
        const auto token = this->lyricsWork.Begin();
        this->SetState(State::Loading);
        api::ReadTrack(this->musikcore, *track, { MC_TRACK_FIELD_TITLE, MC_TRACK_FIELD_ARTIST, MC_TRACK_FIELD_LYRICS },
            this->lyricsWork.Wrap([this, token](mc_result status, api::TrackPage page, std::string_view) {
                if (status != MC_OK || !page.Size()) {
                    this->SetState(State::Failed);
                    return;
                }

                const auto metadata = page.Track(0);
                this->trackTitle = metadata.Title();
                this->trackArtist = metadata.Artist();
                const auto lyrics = metadata.Value(MC_TRACK_FIELD_LYRICS);
                if (!lyrics.empty()) {
                    this->OnLyricsLoaded(std::string(lyrics));
                    return;
                }

                /* The HTTP callback only posts data. Check the request token on the UI thread. */
                auto& queue = this->MessageQueue();
                auddio::FindLyrics(this->trackArtist, this->trackTitle, [&queue, target = this, token](std::string lyrics) {
                    queue.Post(std::make_shared<LyricsLoadedMessage>(target, token, std::move(lyrics)));
                });
            }));
    }
    catch (const api::Error& error) {
        musik::debug::error("LyricsLayout", error.what());
        this->SetState(State::Failed);
    }
}

void LyricsLayout::UpdateAdapter() {
    std::string fixed = this->currentLyrics;
    str::ReplaceAll(fixed, "\r\n", "\n");
    str::ReplaceAll(fixed, "\r", "\n");
    auto items = str::Split(fixed, "\n");
    this->adapter->Clear();
    for (auto& text : items) {
        this->adapter->AddEntry(std::make_shared<SingleLineEntry>(text));
    }
}

void LyricsLayout::SetState(State state) {
    switch (state) {
        case State::NotPlaying: {
                this->listView->Hide();
                this->infoText->Show();
                this->infoText->SetText(_TSTR("lyrics_not_playing"));
                this->currentTrackId = -1LL;
            }
            break;
        case State::Loading: {
                this->listView->Hide();
                this->infoText->Show();
                this->infoText->SetText(_TSTR("lyrics_loading"));
            }
            break;
        case State::Loaded: {
                this->infoText->Hide();
                this->listView->Show();
                if (this->IsVisible()) {
                    this->listView->Focus();
                }
            }
            break;
        case State::Failed: {
                this->listView->Hide();
                this->infoText->Show();
                this->infoText->SetText(u8fmt(
                    _TSTR("lyrics_lookup_failed"),
                    Hotkeys::Get(Hotkeys::LyricsRetry).c_str()));
                this->currentTrackId = -1LL;
            }
            break;
    }
}

void LyricsLayout::SetShortcutsWindow(ShortcutsWindow* shortcuts) {
    if (shortcuts) {
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLyrics), _TSTR("shortcuts_lyrics"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibrary), _TSTR("shortcuts_library"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibraryPlayQueue), _TSTR("shortcuts_play_queue"));
        shortcuts->AddShortcut(App::Instance().GetQuitKey(), _TSTR("shortcuts_quit"));

        shortcuts->SetChangedCallback([this](std::string key) {
            if (key == App::Instance().GetQuitKey()) {
                App::Instance().Quit();
            }
            else {
                this->KeyPress(key);
            }
        });

        shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateLyrics));
    }
}
