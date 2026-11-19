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
#include <cstdlib>

#include <cursespp/Colors.h>
#include <cursespp/App.h>
#include <cursespp/Screen.h>

#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/track/LibraryTrack.h>
#include <musikcore/library/track/TrackPageCache.h>
#include <musikcore/library.hpp>
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/support/PreferenceKeys.h>

#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <app/util/Playback.h>
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
constexpr int kSearchPagePollIntervalMs = 2;

/* Owns the search snapshot and its bounded window. Neither copyable nor movable. */
struct TrackSearchLayout::PageCache {
    explicit PageCache(client::TrackResult result) : result(std::move(result)) {}
    PageCache(const PageCache&) = delete;
    PageCache& operator=(const PageCache&) = delete;

    client::TrackResult result;
    TrackPageCache pages;
    size_t pageSize = 1;
    size_t firstRow = 0;
    size_t lastRow = 0;
    bool forward = true;
    bool viewportKnown = false;
    mc_request_id request = 0;
    size_t pending = 0;
    std::optional<size_t> failed;
};

static TrackSortType getDefaultTrackSort(std::shared_ptr<musik::core::Preferences> prefs) {
    const auto sort = (TrackSortType) prefs->GetInt(
        keys::TrackSearchSortOrder, (int)TrackSortType::Album);
    return kTrackListOrderByToDisplayKey.count(sort) ? sort : TrackSortType::Album;
}

TrackSearchLayout::TrackSearchLayout(
    musik::core::audio::PlaybackService& playback,
    musik::core::ILibraryPtr library,
    std::shared_ptr<musik::core::client::Context> apiContext,
    std::shared_ptr<const musik::core::client::Library> apiLibrary)
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
    /* The base layout can redraw child windows while releasing them. */
    this->trackList->DetachRowSource();
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
    this->trackList->MouseEvent.connect(this, &TrackSearchLayout::OnWindowMouseEvent);
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
        this->CancelLocalSearch();
        this->ClearPages();
        this->pageCache.reset();
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

    if (!this->apiLibrary) {
        /* Use the legacy query if the API handle is unavailable. */
        this->trackList->Requery(
            std::make_shared<SearchTrackListQuery>(this->library, this->matchType, filter, sortOrder));
        return;
    }

    try {
        const auto mode = this->matchType == MatchType::Regex ? MC_TRACK_SEARCH_REGEX : MC_TRACK_SEARCH_SUBSTRING;
        const auto generation = this->localSearchGeneration.Capture();
        const auto hash = std::hash<std::string>{}(filter);
        this->localSearchRequest = client::OpenTrackSearch(*this->apiContext, *this->apiLibrary,
            client::TrackSearch{ filter, mode, static_cast<mc_track_search_order>(sortOrder) },
            [this, generation, sortOrder, hash](mc_result status, client::TrackResult result, std::string_view) {
                /* Cancellation cannot retract a completion already queued for dispatch. */
                if (!generation.IsCurrent()) return;
                this->localSearchRequest = 0;
                if (status != MC_OK) return;

                auto headers = std::make_shared<std::set<size_t>>();
                auto durations = std::make_shared<std::map<size_t, size_t>>();
                for (size_t row = 0; row < result.Size(); ++row) {
                    if (result.StartsAlbum(row)) {
                        headers->insert(row);
                        durations->emplace(row, result.AlbumDurationSeconds(row));
                    }
                }

                this->ClearPages();
                if (std::getenv("MUSIKCUBE_SEARCH_LEGACY_ROWS")) {
                    /* Keep the previous renderer input available for production A/B runs. */
                    auto tracks = std::make_shared<TrackList>(this->library);
                    for (size_t row = 0; row < result.Size(); ++row) tracks->Add(result.Id(row));
                    this->pageCache.reset();
                    this->trackList->SetQueryResult(std::move(tracks), std::move(headers), std::move(durations), hash);
                } else {
                    this->pageCache = std::make_unique<PageCache>(std::move(result));
                    this->trackList->SetQueryResult(TrackListView::RowSource{
                        this->pageCache->result.Size(),
                        [this](size_t row) { return this->pageCache->result.Id(row); },
                        [this](size_t row) { return this->DisplayTrack(row); },
                        [this](size_t first, size_t last, size_t height) { this->SetViewport(first, last, height); },
                        [this] { this->ClearPages(); this->Post(message::LoadTrackSearchPage); },
                    }, std::move(headers), std::move(durations), hash);
                }
                this->SetSearchTitle(_TSTR(kTrackListOrderByToDisplayKey.at(sortOrder)));
            });
    } catch (const client::Error& error) {
        musik::debug::error("TrackSearchLayout", error.what());
    }
}

void TrackSearchLayout::CancelSearchRequests() {
    this->CancelLocalSearch();
    this->ClearPages();
}

void TrackSearchLayout::CancelLocalSearch() {
    this->localSearchGeneration.Invalidate();
    if (this->localSearchRequest) {
        this->apiContext->Cancel(this->localSearchRequest);
        this->localSearchRequest = 0;
    }
}

void TrackSearchLayout::ClearPages() {
    this->pageGeneration.Invalidate();
    if (!this->pageCache) return;
    auto& cache = *this->pageCache;
    App::Instance().SetInputPollInterval(0);
    if (cache.request) this->apiContext->Cancel(cache.request);
    cache.request = 0;
    cache.pages.Clear();
    cache.failed.reset();
}

void TrackSearchLayout::SetViewport(size_t first, size_t last, size_t height) {
    auto& cache = *this->pageCache;
    if (cache.pageSize != height) {
        this->ClearPages();
        cache.pageSize = height;
        cache.viewportKnown = false;
    }
    if (!cache.viewportKnown || cache.firstRow != first || cache.lastRow != last) {
        if (first != cache.firstRow) cache.forward = first > cache.firstRow;
        cache.firstRow = first;
        cache.lastRow = last;
        cache.viewportKnown = true;
        cache.failed.reset();
        App::Instance().SetInputPollInterval(kSearchPagePollIntervalMs);
        this->Post(message::LoadTrackSearchPage);
    }
}

void TrackSearchLayout::LoadPage() {
    if (!this->pageCache || !this->IsVisible()) return;
    auto& cache = *this->pageCache;
    if (!cache.request) App::Instance().SetInputPollInterval(0);
    const auto count = cache.result.Size();
    if (!cache.viewportKnown || cache.firstRow >= count) return;

    const auto size = cache.pageSize;
    const auto first = (cache.firstRow / size) * size;
    const auto last = (std::min(cache.lastRow, count - 1) / size) * size;
    auto next = first;
    while (next <= last && cache.pages.Find(next)) next += size;

    /* Serve missing visible rows before fetching one page in the scroll direction. */
    if (cache.request) {
        if (next <= last && (cache.pending < first || cache.pending > last)) {
            this->apiContext->Cancel(cache.request);
        }
        return;
    }
    if (next > last) {
        if (!cache.forward && first == 0) return;
        next = cache.forward ? last + size : first - size;
    }
    if (next >= count || cache.pages.Find(next) || cache.failed == next) return;

    const auto generation = this->pageGeneration.Capture();
    cache.pending = next;
    App::Instance().SetInputPollInterval(kSearchPagePollIntervalMs);
    cache.request = client::ReadTracks(*this->apiContext, cache.result, client::TrackRange{
        next, size, { MC_TRACK_FIELD_ID, MC_TRACK_FIELD_TITLE, MC_TRACK_FIELD_ARTIST,
            MC_TRACK_FIELD_ALBUM, MC_TRACK_FIELD_DURATION, MC_TRACK_FIELD_TRACK, MC_TRACK_FIELD_RATING } },
        [this, generation, next](mc_result status, client::TrackPage page, std::string_view) {
            if (!generation.IsCurrent()) return;
            auto& cache = *this->pageCache;
            cache.request = 0;
            if (status == MC_OK) {
                const auto firstPage = (cache.firstRow / cache.pageSize) * cache.pageSize;
                const auto lastPage = (cache.lastRow / cache.pageSize) * cache.pageSize;
                if (cache.pages.Store(next, std::move(page), firstPage, lastPage) &&
                    firstPage <= next && next <= lastPage) {
                    this->trackList->Redraw();
                }
            } else if (status != MC_ERROR_CANCELLED) {
                cache.failed = next;
            }
            this->LoadPage();
        });
}

TrackPtr TrackSearchLayout::DisplayTrack(size_t row) {
    const auto& cache = *this->pageCache;
    auto track = std::make_shared<LibraryTrack>(cache.result.Id(row), this->library);
    const auto offset = (row / cache.pageSize) * cache.pageSize;
    const auto* page = cache.pages.Find(offset);
    if (!page) return track;

    /* Adapt only the rendered row. The page remains the sole display cache. */
    const auto metadata = page->Track(row - offset);
    if (metadata.Value(MC_TRACK_FIELD_ID).data()) {
        for (const auto* key : { MC_TRACK_FIELD_TITLE, MC_TRACK_FIELD_ARTIST, MC_TRACK_FIELD_ALBUM,
                 MC_TRACK_FIELD_DURATION, MC_TRACK_FIELD_TRACK, MC_TRACK_FIELD_RATING }) {
            track->SetValue(key, metadata.Value(key).data());
        }
        track->SetMetadataState(musik::core::sdk::MetadataState::Loaded);
    }
    return track;
}

void TrackSearchLayout::PlayFromTop() {
    playback::PlayFromTop(*this->trackList, this->playback);
}

void TrackSearchLayout::ProcessMessage(IMessage &message) {
    if (message.Type() == message::RequeryTrackList) {
        if (this->IsVisible()) this->Requery();
    }
    else if (message.Type() == message::LoadTrackSearchPage) {
        this->LoadPage();
    }
    else {
        LayoutBase::ProcessMessage(message);
    }
}

void TrackSearchLayout::OnRequeried(TrackListQueryBase* query) {
    auto searchQuery = dynamic_cast<SearchTrackListQuery*>(query);
    if (searchQuery) {
        this->SetSearchTitle(searchQuery->GetSortDisplayString());
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
        playback::PlaySelected(*(this->trackList.get()), this->playback);
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
