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

#include <cursespp/Colors.h>
#include <cursespp/App.h>
#include <cursespp/Screen.h>


#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <app/util/PreferenceKeys.h>
#include <app/util/WindowUtil.h>
#include <app/overlay/PlayQueueOverlays.h>
#include <app/overlay/TrackOverlays.h>

#include "TrackSearchLayout.h"

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::core::runtime;
using namespace musik::cube;
using namespace cursespp;

namespace keys = musik::cube::prefs::keys;

constexpr int kSearchHeight = 3;
constexpr int kRequeryIntervalMs = 300;

static mc_track_search_order getDefaultTrackSort(const api::Preferences& prefs) {
    const auto sort = (mc_track_search_order) prefs.GetInt(
        keys::TrackSearchSortOrder, (int) MC_TRACK_SEARCH_ALBUM);
    return sort <= MC_TRACK_SEARCH_GENRE ? sort : MC_TRACK_SEARCH_ALBUM;
}

TrackSearchLayout::TrackSearchLayout(
    std::shared_ptr<TrackListView> trackList,
    musik::core::api::Context& musikcore)
: LayoutBase()
, trackList(std::move(trackList))
, musikcore(musikcore)
{
    this->InitializeWindows();
}

TrackSearchLayout::~TrackSearchLayout() {
    this->CancelSearchRequests();
    this->SaveSession();
}

void TrackSearchLayout::LoadLastSession() {
    api::Preferences session { MC_PREFERENCE_SESSION };

    const std::string lastFilter = session.GetString(keys::LastTrackFilter);
    if (lastFilter.size()) {
        this->input->SetText(lastFilter);
    }

    this->matchType = session.GetInt(keys::LastTrackFilterMatchType, musik::cube::prefs::SearchSubstring) == musik::cube::prefs::SearchRegex
        ? MC_TRACK_SEARCH_REGEX
        : MC_TRACK_SEARCH_SUBSTRING;
}

void TrackSearchLayout::SaveSession() {
    api::Preferences session { MC_PREFERENCE_SESSION };
    session.SetString(keys::LastTrackFilter, this->input->GetText());
    session.SetInt(keys::LastTrackFilterMatchType,
        this->matchType == MC_TRACK_SEARCH_REGEX ? musik::cube::prefs::SearchRegex : musik::cube::prefs::SearchSubstring);
}

void TrackSearchLayout::OnLayout() {
    const int cx = this->GetWidth(), cy = this->GetHeight();
    const int x = 0, y = 0;

    const int inputWidth = cx / 2;
    const int inputX = x + ((cx - inputWidth) / 2);
    this->input->MoveAndResize(inputX, y, cx / 2, kSearchHeight);

    const bool inputIsRegex = this->matchType == MC_TRACK_SEARCH_REGEX;
    this->input->SetHint(_TSTR(inputIsRegex ? "search_regex_hint" : "search_filter_hint"));
    this->input->SetFocusedFrameColor(inputIsRegex ? Color::FrameImportant : Color::FrameFocused);

    this->trackList->MoveAndResize(
        x,
        y + kSearchHeight,
        this->GetWidth(),
        this->GetHeight() - kSearchHeight);
}

void TrackSearchLayout::OnIndexerFinished(int totalUrisScanned) {
    this->Post(message::RequeryTrackList);
}

void TrackSearchLayout::InitializeWindows() {
    this->input = std::make_shared<TextInput>();
    this->input->TextChanged.connect(this, &TrackSearchLayout::OnInputChanged);
    this->input->EnterPressed.connect(this, &TrackSearchLayout::OnEnterPressed);
    this->input->SetFocusOrder(0);
    this->AddWindow(this->input);

    this->trackList->MouseEvent.connect(this, &TrackSearchLayout::OnWindowMouseEvent);
    this->trackList->SetFocusOrder(1);
    this->trackList->SetAllowArrowKeyPropagation();
    this->AddWindow(this->trackList);
}

void TrackSearchLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    if (visible) {
        this->Requery();
    }
    else {
        this->CancelSearch();
        this->SaveSession();
        this->input->SetText("");
        this->trackList->Reset();
        this->SetFocusIndex(0, false);
    }
}

void TrackSearchLayout::FocusInput() {
    this->SetFocus(this->input);
}

void TrackSearchLayout::Requery() {
    const std::string& filter = this->input->GetText();
    const mc_track_search_order sortOrder = getDefaultTrackSort(this->prefs);
    this->CancelSearch();


    try {
        const auto hash = std::hash<std::string> {}(filter);
        this->searchRequest = api::OpenTrackSearch(this->musikcore,
            api::TrackSearch { filter, this->matchType, sortOrder },
            this->searchWork.Begin([this, sortOrder, hash](mc_result status, api::TrackResult result, std::string_view) {
                this->searchRequest = 0;
                if (status == MC_ERROR_BUSY) {
                    this->Debounce(message::RequeryTrackList, 0, 0, kRequeryIntervalMs);
                }
                if (status != MC_OK) {
                    return;
                }

                this->trackList->SetTrackResult(std::move(result), hash);
                this->SetSearchTitle(TrackOverlays::TrackSortName(sortOrder));
            }));
    }
    catch (const api::Error& error) {
        musik::core::api::WriteLog(MC_LOG_ERROR, "TrackSearchLayout", error.what());
    }
}

void TrackSearchLayout::CancelSearchRequests() {
    this->CancelSearch();
    this->trackList->CancelTrackRequests();
}

void TrackSearchLayout::CancelSearch() {
    this->searchWork.Invalidate();
    if (this->searchRequest) {
        this->musikcore.Cancel(this->searchRequest);
        this->searchRequest = 0;
    }
}

void TrackSearchLayout::PlayFromTop() {
    this->trackList->PlayRow(0);
}

void TrackSearchLayout::ProcessMessage(IMessage& message) {
    if (message.Type() == message::RequeryTrackList) {
        if (this->IsVisible()) {
            this->Requery();
        }
    }
    else {
        LayoutBase::ProcessMessage(message);
    }
}

void TrackSearchLayout::SetSearchTitle(const std::string& order) {
    this->trackList->SetFrameTitle(u8fmt(_TSTR("track_filter_title"), order.c_str()));
}

void TrackSearchLayout::OnInputChanged(cursespp::TextInput* sender, std::string value) {
    if (this->IsVisible()) {
        Debounce(message::RequeryTrackList, 0, 0, kRequeryIntervalMs);
    }
}

void TrackSearchLayout::OnEnterPressed(cursespp::TextInput* sender) {
    if (this->trackList->TrackCount()) {
        this->PlayFromTop();
        this->SetFocus(this->trackList);
    }
}

void TrackSearchLayout::OnWindowMouseEvent(Window* window, const IMouseHandler::Event* mouseEvent) {
    if (windowutil::WasHeaderClicked(window, mouseEvent)) {
        this->ShowTrackSortOverlay();
    }
}

void TrackSearchLayout::ShowTrackSortOverlay() {
    TrackOverlays::ShowTrackSearchSortOverlay(
        getDefaultTrackSort(this->prefs),
        [this](mc_track_search_order type) {
            this->prefs.SetInt(keys::TrackSearchSortOrder, (int) type);
            this->Requery();
        });
}

void TrackSearchLayout::ToggleMatchType() {
    const bool isRegex = this->matchType == MC_TRACK_SEARCH_REGEX;
    this->SetMatchType(isRegex ? MC_TRACK_SEARCH_SUBSTRING : MC_TRACK_SEARCH_REGEX);
}

void TrackSearchLayout::SetMatchType(mc_track_search_mode matchType) {
    if (matchType != this->matchType) {
        this->matchType = matchType;
        this->Layout();
        this->Requery();
    }
}

bool TrackSearchLayout::KeyPress(const std::string& key) {
    if (Hotkeys::Is(Hotkeys::Down, key)) {
        if (this->GetFocus() == this->input) {
            this->FocusNext();
            return true;
        }
    }
    else if (Hotkeys::Is(Hotkeys::Up, key)) {
        if (this->GetFocus() == this->trackList) {
            this->SetFocus(this->input);
            return true;
        }
    }
    else if (Hotkeys::Is(Hotkeys::TrackListChangeSortOrder, key)) {
        this->ShowTrackSortOverlay();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::SearchInputToggleMatchType, key) && this->input->IsFocused()) {
        this->ToggleMatchType();
        return true;
    }
    return LayoutBase::KeyPress(key);
}
