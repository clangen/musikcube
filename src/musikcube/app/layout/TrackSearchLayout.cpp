//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include <cursespp/Colors.h>
#include <cursespp/Screen.h>

#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/support/PreferenceKeys.h>

#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <app/util/Playback.h>
#include <app/util/PreferenceKeys.h>
#include <app/overlay/PlayQueueOverlays.h>
#include <app/overlay/TrackOverlays.h>

#include "TrackSearchLayout.h"

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library::query;
using namespace musik::core::library;
using namespace musik::core::library::constants;
using namespace musik::core::runtime;
using namespace musik::cube;
using namespace cursespp;

namespace keys = musik::cube::prefs::keys;
namespace components = musik::core::prefs::components;

constexpr int kSearchHeight = 3;
constexpr int kRequeryIntervalMs = 300;

static TrackSortType getDefaultTrackSort(std::shared_ptr<musik::core::Preferences> prefs) {
    return (TrackSortType) prefs->GetInt(
        keys::TrackSearchSortOrder, (int)TrackSortType::Album);
}

TrackSearchLayout::TrackSearchLayout(
    musik::core::audio::PlaybackService& playback,
    musik::core::ILibraryPtr library)
: LayoutBase()
, playback(playback)
, library(library) {
    this->prefs = Preferences::ForComponent(components::Settings);
    this->library->Indexer()->Finished.connect(this, &TrackSearchLayout::OnIndexerFinished);
    this->InitializeWindows();
}

TrackSearchLayout::~TrackSearchLayout() {
    this->SaveSession();
}

void TrackSearchLayout::LoadLastSession() {
    auto session = Preferences::ForComponent(components::Session);

    const std::string lastFilter = session->GetString(keys::LastTrackFilter);
    if (lastFilter.size()) {
        this->input->SetText(lastFilter);
    }

    this->matchType = static_cast<MatchType>(session->GetInt(
        keys::LastTrackFilterMatchType,
        static_cast<int>(MatchType::Substring)));
}

void TrackSearchLayout::SaveSession() {
    auto session = Preferences::ForComponent(components::Session);
    session->SetString(keys::LastTrackFilter, this->input->GetText().c_str());
    session->SetInt(keys::LastTrackFilterMatchType, static_cast<int>(this->matchType));
}

void TrackSearchLayout::OnLayout() {
    const int cx = this->GetWidth(), cy = this->GetHeight();
    const int x = 0, y = 0;

    const int inputWidth = cx / 2;
    const int inputX = x + ((cx - inputWidth) / 2);
    this->input->MoveAndResize(inputX, y, cx / 2, kSearchHeight);

    const bool inputIsRegex = this->matchType == MatchType::Regex;
    this->input->SetHint(_TSTR(inputIsRegex ? "search_regex_hint" : "search_filter_hint"));
    this->input->SetFocusedFrameColor(inputIsRegex ? Color::FrameImportant : Color::FrameFocused);

    this->trackList->MoveAndResize(
        x,
        y + kSearchHeight,
        this->GetWidth(),
        this->GetHeight() - kSearchHeight);
}

void TrackSearchLayout::OnIndexerFinished(int totalUrisScanned){
        this->Requery();
}

void TrackSearchLayout::InitializeWindows() {
    this->input = std::make_shared<TextInput>();
    this->input->TextChanged.connect(this, &TrackSearchLayout::OnInputChanged);
    this->input->EnterPressed.connect(this, &TrackSearchLayout::OnEnterPressed);
    this->input->SetFocusOrder(0);
    this->AddWindow(this->input);

    this->trackList = std::make_shared<TrackListView>(this->playback, this->library);
    this->trackList->SetFocusOrder(1);
    this->trackList->SetAllowArrowKeyPropagation();
    this->trackList->Requeried.connect(this, &TrackSearchLayout::OnRequeried);
    this->AddWindow(this->trackList);
}

void TrackSearchLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    if (visible) {
        this->Requery();
    }
    else {
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
    const TrackSortType sortOrder = getDefaultTrackSort(this->prefs);
    this->trackList->Requery(
        std::make_shared<SearchTrackListQuery>(
            this->library,
            this->matchType,
            filter,
            sortOrder));
}

void TrackSearchLayout::PlayFromTop() {
    playback::PlayFromTop(*this->trackList, this->playback);
}

void TrackSearchLayout::ProcessMessage(IMessage &message) {
    if (message.Type() == message::RequeryTrackList) {
        this->Requery();
    }
    else {
        LayoutBase::ProcessMessage(message);
    }
}

void TrackSearchLayout::OnRequeried(TrackListQueryBase* query) {
    auto searchQuery = dynamic_cast<SearchTrackListQuery*>(query);
    if (searchQuery) {
        this->trackList->SetFrameTitle(u8fmt(
            _TSTR("track_filter_title"),
            searchQuery->GetSortDisplayString().c_str()));
    }
}

void TrackSearchLayout::OnInputChanged(cursespp::TextInput* sender, std::string value) {
    if (this->IsVisible()) {
        Debounce(message::RequeryTrackList, 0, 0, kRequeryIntervalMs);
    }
}

void TrackSearchLayout::OnEnterPressed(cursespp::TextInput* sender) {
    if (this->trackList->GetTrackList()->Count()) {
        playback::PlaySelected(*(this->trackList.get()), this->playback);
        this->SetFocus(this->trackList);
    }
}

void TrackSearchLayout::ToggleMatchType() {
    const bool isRegex = this->matchType == MatchType::Regex;
    this->SetMatchType(isRegex ? MatchType::Substring : MatchType::Regex);
}

void TrackSearchLayout::SetMatchType(MatchType matchType) {
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
        TrackOverlays::ShowTrackSearchSortOverlay(
            getDefaultTrackSort(this->prefs),
            kTrackListOrderByToDisplayKey,
            [this](TrackSortType type) {
                this->prefs->SetInt(keys::TrackSearchSortOrder, (int)type);
                this->Requery();
            });
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::SearchInputToggleMatchType, key) && this->input->IsFocused()) {
        this->ToggleMatchType();
        return true;
    }
    return LayoutBase::KeyPress(key);
}
