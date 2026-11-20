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

#include <cursespp/Colors.h>
#include <cursespp/App.h>
#include <cursespp/Screen.h>

#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/support/PreferenceKeys.h>

#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <app/util/PreferenceKeys.h>
#include <app/util/WindowUtil.h>
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
    const auto sort = (TrackSortType) prefs->GetInt(
        keys::TrackSearchSortOrder, (int)TrackSortType::Album);
    return kTrackListOrderByToDisplayKey.count(sort) ? sort : TrackSortType::Album;
}

TrackSearchLayout::TrackSearchLayout(
    musik::core::audio::PlaybackService& playback,
    musik::core::ILibraryPtr library,
    std::shared_ptr<musik::core::api::Context> apiContext,
    std::shared_ptr<const musik::core::api::Library> apiLibrary)
: LayoutBase()
, playback(playback)
, library(library)
, apiContext(std::move(apiContext))
, apiLibrary(std::move(apiLibrary)) {
    this->prefs = Preferences::ForComponent(components::Settings);
    this->library->Indexer()->Finished.connect(this, &TrackSearchLayout::OnIndexerFinished);
    this->InitializeWindows();
}

TrackSearchLayout::~TrackSearchLayout() {
    this->CancelSearchRequests();
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

void TrackSearchLayout::OnIndexerFinished(int totalUrisScanned) {
    this->Post(message::RequeryTrackList);
}

void TrackSearchLayout::InitializeWindows() {
    this->input = std::make_shared<TextInput>();
    this->input->TextChanged.connect(this, &TrackSearchLayout::OnInputChanged);
    this->input->EnterPressed.connect(this, &TrackSearchLayout::OnEnterPressed);
    this->input->SetFocusOrder(0);
    this->AddWindow(this->input);

    this->trackList = std::make_shared<TrackListView>(this->playback, this->library);
    this->trackList->SetApiLibrary(this->apiContext, this->apiLibrary);
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
        this->CancelLocalSearch();
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
    this->CancelLocalSearch();

    if (!this->apiLibrary) return;

    try {
        const auto mode = this->matchType == MatchType::Regex ? MC_TRACK_SEARCH_REGEX : MC_TRACK_SEARCH_SUBSTRING;
        const auto generation = this->localSearchGeneration.Capture();
        const auto hash = std::hash<std::string>{}(filter);
        this->localSearchRequest = api::OpenTrackSearch(*this->apiContext, *this->apiLibrary,
            api::TrackSearch{ filter, mode, static_cast<mc_track_search_order>(sortOrder) },
            [this, generation, sortOrder, hash](mc_result status, api::TrackResult result, std::string_view) {
                /* Cancellation cannot retract a completion already queued for dispatch. */
                if (!generation.IsCurrent()) return;
                this->localSearchRequest = 0;
                if (status == MC_ERROR_BUSY) this->Debounce(message::RequeryTrackList, 0, 0, kRequeryIntervalMs);
                if (status != MC_OK) return;

                this->trackList->SetTrackResult(std::move(result), hash);
                this->SetSearchTitle(_TSTR(kTrackListOrderByToDisplayKey.at(sortOrder)));
            });
    } catch (const api::Error& error) {
        musik::debug::error("TrackSearchLayout", error.what());
    }
}

void TrackSearchLayout::CancelSearchRequests() {
    this->CancelLocalSearch();
    this->trackList->CancelTrackRequests();
}

void TrackSearchLayout::CancelLocalSearch() {
    this->localSearchGeneration.Invalidate();
    if (this->localSearchRequest) {
        this->apiContext->Cancel(this->localSearchRequest);
        this->localSearchRequest = 0;
    }
}

void TrackSearchLayout::PlayFromTop() {
    this->trackList->PlayRow(0);
}

void TrackSearchLayout::ProcessMessage(IMessage &message) {
    if (message.Type() == message::RequeryTrackList) {
        if (this->IsVisible()) this->Requery();
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
        kTrackListOrderByToDisplayKey,
        [this](TrackSortType type) {
            this->prefs->SetInt(keys::TrackSearchSortOrder, (int)type);
            this->Requery();
        });
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
        this->ShowTrackSortOverlay();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::SearchInputToggleMatchType, key) && this->input->IsFocused()) {
        this->ToggleMatchType();
        return true;
    }
    return LayoutBase::KeyPress(key);
}
