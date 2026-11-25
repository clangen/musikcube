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
#include <musikcore/log.hpp>
#include <app/layout/LyricsLayout.h>
#include <app/util/Messages.h>
#include <app/util/Locale.h>
#include <musikcore/online.hpp>
#include <common/String.h>
#include <cursespp/App.h>
#include <cursespp/Screen.h>
#include <cursespp/ToastOverlay.h>
#include <cursespp/SingleLineEntry.h>
#include <app/util/Hotkeys.h>

using namespace musik::cube;
using namespace musik::core;
using namespace musik::core::runtime;
using namespace musik::core::sdk;
using namespace cursespp;

LyricsLayout::LyricsLayout(api::Context& musikcore)
: LayoutBase()
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
        /* A playback notification supersedes the old track, even across equal catalog IDs. */
        view.refreshLyrics = true;
        if (view.IsVisible()) {
            view.LoadLyricsForCurrentTrack();
        }
        else {
            view.CancelLyrics();
        }
    }, this, { MC_PLAYBACK_CHANGED_TRACK });
}

LyricsLayout::~LyricsLayout() {
    this->playback.SetChangedCallback(nullptr);
    this->CancelLyrics();
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
        if (this->refreshLyrics || this->state == State::Failed) {
            this->LoadLyricsForCurrentTrack();
        }
        this->FocusFirst();
    }
    else if (this->state == State::Loading) {
        this->CancelLyrics();
        this->refreshLyrics = true;
    }
}

void LyricsLayout::CancelLyrics() {
    this->lyricsWork.Invalidate();
    if (this->lyricsRequest) {
        this->musikcore.Cancel(std::exchange(this->lyricsRequest, 0));
    }
}

void LyricsLayout::LoadLyricsForCurrentTrack() {
    this->CancelLyrics();
    this->refreshLyrics = false;

    try {
        const auto track = this->playback.CurrentTrack();
        if (!track) {
            this->SetState(State::NotPlaying);
            return;
        }

        /* The engine already has the display tags. Only lyrics need a catalog read. */
        const auto metadata = this->playback.CurrentTrackMetadata();
        this->trackTitle = metadata ? metadata->Title() : std::string_view {};
        this->trackArtist = metadata ? metadata->Artist() : std::string_view {};
        this->SetState(State::Loading);
        this->lyricsRequest = api::ReadTrack(this->musikcore, *track, { MC_TRACK_FIELD_LYRICS },
            this->lyricsWork.Wrap([this](mc_result status, api::TrackPage page, std::string_view) {
                this->lyricsRequest = 0;
                if (status != MC_OK || !page.Size()) {
                    this->SetState(State::Failed);
                    return;
                }

                const auto lyrics = page.Track(0).Value(MC_TRACK_FIELD_LYRICS);
                if (!lyrics.empty()) {
                    this->OnLyricsLoaded(std::string(lyrics));
                    return;
                }

                try {
                    this->lyricsRequest = api::FindLyrics(this->musikcore, this->trackArtist, this->trackTitle,
                        this->lyricsWork.Wrap([this](mc_result status, std::string_view text) {
                            this->lyricsRequest = 0;
                            this->OnLyricsLoaded(status == MC_OK ? std::string(text) : std::string{});
                        }));
                }
                catch (const api::Error& error) {
                    api::WriteLog(MC_LOG_ERROR, "LyricsLayout", error.what());
                    this->SetState(State::Failed);
                }
            }));
    }
    catch (const api::Error& error) {
        musik::core::api::WriteLog(MC_LOG_ERROR, "LyricsLayout", error.what());
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
    this->state = state;
    switch (state) {
        case State::NotPlaying: {
                this->listView->Hide();
                this->infoText->Show();
                this->infoText->SetText(_TSTR("lyrics_not_playing"));
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
