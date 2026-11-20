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
#include "TrackListView.h"

#include <cursespp/Colors.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>
#include <cursespp/ToastOverlay.h>

#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/support/Duration.h>

#include <app/util/Hotkeys.h>
#include <app/util/Playback.h>
#include <app/overlay/PlayQueueOverlays.h>
#include <app/overlay/TrackOverlays.h>
#include <musikcore/ffi/consumer/playback-binding.h>
#include <musikcore/library/track/LibraryTrack.h>
#include <musikcore/debug.h>

static constexpr int kTrackPagePollIntervalMs = 2;
static constexpr int kTrackPageRetryMs = 25;

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::core::runtime;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace cursespp;

using namespace std::chrono;

/* if the user hasn't changed the selected index in 60 seconds
we assume he's not paying attention, and will automatically scroll
the view to the playing track if it's invisible. */
static const milliseconds AUTO_SCROLL_COOLDOWN = milliseconds(60000LL);

static inline milliseconds now() noexcept {
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch());
}

TrackListView::TrackListView(
    PlaybackService& playback,
    ILibraryPtr library,
    RowDecorator decorator)
: ListWindow(nullptr)
, playback(playback)
, apiPlayback(CreatePlaybackHandle(playback)) {
    this->library = library;
    this->playback.TrackChanged.connect(this, &TrackListView::OnTrackChanged);
    this->adapter = std::make_unique<Adapter>(*this);
    this->lastQueryHash = 0;
    this->lastChanged = now();
    this->decorator = decorator;
    this->trackNumType = TrackRowRenderers::TrackNumType::Metadata;
    this->renderer = TrackRowRenderers::Get(TrackRowRenderers::Type::AlbumSort);
    this->playing = playback.GetPlaying();
}

TrackListView::~TrackListView() {
    this->CancelTrackRequests();
}

void TrackListView::SetApiLibrary(std::shared_ptr<api::Context> context, std::shared_ptr<const api::Library> library) {
    this->apiContext = std::move(context);
    this->apiLibrary = std::move(library);
}

void TrackListView::CancelTrackRequest() {
    this->trackRequestGeneration.Invalidate();
    if (this->trackRequest) this->apiContext->Cancel(std::exchange(this->trackRequest, 0));
    this->pendingTrackQuery = {};
}

void TrackListView::CancelTrackRequests() {
    this->CancelTrackRequest();
    if (this->resultWindow) this->resultWindow->Clear();
    this->UpdatePagePolling(false);
}

void TrackListView::RequeryTracks(size_t hash, TrackRequest request) {
    this->CancelTrackRequest();
    if (!this->apiLibrary) {
        this->Reset();
        return;
    }
    if (hash != this->lastQueryHash) this->Reset();

    this->pendingTrackQuery = std::move(request);
    this->pendingTrackHash = hash;
    this->SubmitTrackRequest();
}

void TrackListView::SubmitTrackRequest() {
    if (!this->pendingTrackQuery || this->trackRequest) return;
    const auto ticket = this->trackRequestGeneration.Capture();
    try {
        this->trackRequest = this->pendingTrackQuery(*this->apiContext, *this->apiLibrary,
            [this, ticket](mc_result status, api::TrackResult result, std::string_view) {
                if (!ticket.IsCurrent()) return;
                this->trackRequest = 0;
                if (status == MC_ERROR_BUSY) {
                    this->Debounce(RetryTrackRequestMessage, 0, 0, kTrackPageRetryMs);
                    return;
                }
                this->pendingTrackQuery = {};
                if (status == MC_OK) this->SetTrackResult(std::move(result), this->pendingTrackHash);
            });
    }
    catch (const api::Error& error) {
        if (error.Result() == MC_ERROR_BUSY) {
            this->Debounce(RetryTrackRequestMessage, 0, 0, kTrackPageRetryMs);
        }
        else {
            this->pendingTrackQuery = {};
            musik::debug::error("TrackListView", error.what());
        }
    }
}

void TrackListView::SetTrackIds(const int64_t* ids, size_t count, size_t hash) {
    if (!this->apiLibrary) {
        this->Reset();
        return;
    }
    this->SetTrackResult(api::MakeTrackResult(*this->apiLibrary, ids, count), hash);
}

void TrackListView::SetTrackResult(api::TrackResult result, size_t hash) {
    const bool hadTracks = this->TrackCount() > 0;
    auto headers = std::make_shared<std::set<size_t>>();
    auto durations = std::make_shared<std::map<size_t, size_t>>();
    for (size_t row = 0; row < result.Size(); ++row) {
        if (result.StartsAlbum(row)) {
            headers->insert(row);
            durations->emplace(row, result.AlbumDurationSeconds(row));
        }
    }

    this->UpdatePagePolling(false);
    this->resultWindow = std::make_unique<api::TrackResultWindow>(*this->apiContext,
        std::move(result), std::vector<std::string>{ MC_TRACK_FIELD_ID, MC_TRACK_FIELD_TITLE, MC_TRACK_FIELD_ARTIST,
            MC_TRACK_FIELD_ALBUM, MC_TRACK_FIELD_DURATION, MC_TRACK_FIELD_TRACK, MC_TRACK_FIELD_RATING },
        [this](size_t offset, size_t count, mc_result status) {
            this->UpdatePagePolling(this->resultWindow->Pending() && this->IsParentVisible());
            if (this->resultWindow->IntersectsViewport(offset, count)) this->Redraw();
            this->HandlePageStatus(status);
        });

    this->tracks.reset();
    this->ApplyQueryResult(std::move(headers), std::move(durations), hash, hadTracks);
}

void TrackListView::OnParentVisibilityChanged(bool visible) {
    ListWindow::OnParentVisibilityChanged(visible);
    if (!visible) this->UpdatePagePolling(false);
    else if (this->resultWindow) this->Post(LoadTrackPageMessage);
}

void TrackListView::UpdatePagePolling(bool pending) {
    if (this->pollingPages == pending) return;
    this->pollingPages = pending;
    App::Instance().SetInputPollInterval(pending ? kTrackPagePollIntervalMs : 0);
}

void TrackListView::HandlePageStatus(mc_result status) {
    if (status == MC_ERROR_BUSY) this->Debounce(LoadTrackPageMessage, 0, 0, kTrackPageRetryMs);
    else if (status != MC_OK && status != MC_ERROR_CANCELLED) {
        musik::debug::error("TrackListView", api::Error(status).what());
    }
}

void TrackListView::LoadTrackPage() {
    if (!this->resultWindow || !this->IsVisible() || !this->IsParentVisible()) return;
    const auto status = this->resultWindow->Load();
    this->UpdatePagePolling(this->resultWindow->Pending());
    this->HandlePageStatus(status);
}

TrackPtr TrackListView::DisplayApiTrack(size_t row) {
    const auto metadata = this->resultWindow->CachedRow(row);
    auto track = std::make_shared<LibraryTrack>(this->resultWindow->Result().Id(row), this->library);
    if (metadata && metadata->Value(MC_TRACK_FIELD_ID).data()) {
        for (const auto* key : { MC_TRACK_FIELD_TITLE, MC_TRACK_FIELD_ARTIST, MC_TRACK_FIELD_ALBUM,
                 MC_TRACK_FIELD_DURATION, MC_TRACK_FIELD_TRACK, MC_TRACK_FIELD_RATING }) {
            const auto value = metadata->Value(key);
            if (value.data()) track->SetValue(key, value.data());
        }
        track->SetMetadataState(MetadataState::Loaded);
    }
    return track;
}

bool TrackListView::PlayRow(size_t row) {
    if (row >= this->TrackCount()) return false;
    if (this->activateQueueRow) {
        this->activateQueueRow(row);
        return true;
    }
    return this->apiPlayback.Play(this->resultWindow->Result(), row) == MC_OK;
}

void TrackListView::SelectFirstTrack() {
    this->SetSelectedIndex(this->headers.HeaderAt(0) ? 1 : 0);
}

void TrackListView::SetTrackNumType(TrackRowRenderers::TrackNumType type) {
    if (this->trackNumType != type) {
        this->trackNumType = type;
        this->OnAdapterChanged();
    }
}

void TrackListView::SetRowRenderer(TrackRowRenderers::Renderer renderer) {
    if (!renderer) {
        throw std::runtime_error("invalid Renderer supplied to TrackListView::SetRowRenderer");
    }
    this->renderer = renderer;
}

void TrackListView::ApplyQueryResult(Headers headers, Durations durations, size_t hash, bool hadTracks) {
    const bool preserveSelection = hadTracks && this->lastQueryHash == hash;
    this->headers.Set(std::move(headers), std::move(durations));
    this->lastQueryHash = hash;
    this->OnAdapterChanged();

    /* A refresh of the same query keeps the user's position. */
    if (!preserveSelection) {
        this->ScrollToTop();
        this->SelectFirstTrack();
    }
}

std::shared_ptr<TrackList> TrackListView::GetTrackList() {
    /* Legacy actions and playlist drafts still consume an ordered list. */
    if (!this->tracks && this->resultWindow) {
        this->tracks = std::make_shared<TrackList>(this->library);
        const auto& result = this->resultWindow->Result();
        for (size_t row = 0; row < result.Size(); ++row) this->tracks->Add(result.Id(row));
    }
    return this->tracks;
}

void TrackListView::Reset() {
    this->CancelTrackRequests();
    this->resultWindow.reset();
    this->tracks.reset();
    this->headers.Reset();
    this->OnAdapterChanged();
}

void TrackListView::InvalidateData() {
    if (this->tracks) this->tracks->ClearCache();
    if (this->resultWindow) {
        this->resultWindow->Clear();
        this->Post(LoadTrackPageMessage);
    }
    this->OnAdapterChanged();
}

TrackPtr TrackListView::GetSelectedTrack() {
    const auto i = this->GetSelectedTrackIndex();
    if (i == ListWindow::NO_SELECTION) return {};
    const auto id = this->resultWindow->Result().Id(i);
    return std::make_shared<TrackList>(this->library, &id, 1)->Get(0);
}

size_t TrackListView::GetSelectedTrackIndex() {
    const auto i = this->GetSelectedIndex();
    if (i == ListWindow::NO_SELECTION || i >= this->EntryCount()) return ListWindow::NO_SELECTION;
    return this->RowIndex(i);
}

size_t TrackListView::RowIndex(size_t adapterIndex) {
    return this->headers.AdapterToTrackListIndex(adapterIndex + this->headers.HeaderAt(adapterIndex));
}

TrackPtr TrackListView::DisplayTrack(size_t row) {
    return row < this->TrackCount() ? this->DisplayApiTrack(row) : TrackPtr{};
}

size_t TrackListView::TrackIndexToAdapterIndex(size_t index) {
    return this->headers.TrackListToAdapterIndex(index);
}

size_t TrackListView::TrackCount() noexcept {
    return this->resultWindow ? this->resultWindow->Result().Size() : 0;
}

size_t TrackListView::EntryCount() {
    return this->GetScrollAdapter().GetEntryCount();
}

void TrackListView::ScrollToPlaying() {
    if (this->playing && this->TrackCount()) {
        const int64_t id = this->playing->GetId();
        for (size_t i = 0; i < this->TrackCount(); i++) {
            const auto rowId = this->resultWindow->Result().Id(i);
            if (rowId == id) {
                const size_t rawIndex = headers.TrackListToAdapterIndex(i);
                this->SetSelectedIndex(rawIndex);

                if (!this->IsEntryVisible(rawIndex)) {
                    this->ScrollTo(rawIndex);
                }

                return;
            }
        }
    }
}

void TrackListView::ProcessMessage(IMessage &message) {
    if (message.Type() == LoadTrackPageMessage) {
        this->LoadTrackPage();
    }
    else if (message.Type() == RetryTrackRequestMessage) {
        this->SubmitTrackRequest();
    }
    else if (message.Type() == ScrollToPlayingMessage) {
        this->ScrollToPlaying();
    }
    else {
        ListWindow::ProcessMessage(message);
    }
}

bool TrackListView::OnEntryActivated(size_t index) {
    if (headers.HeaderAt(this->GetSelectedIndex())) {
        TrackPtr track = this->GetSelectedTrack();
        PlayQueueOverlays::ShowAlbumDividerOverlay(
            MessageQueue(), this->playback, this->library, track, this->addCategoryToPlaylist);
    }
    else {
        this->PlayRow(this->GetSelectedTrackIndex());
    }
    return true;
}

bool TrackListView::OnEntryContextMenu(size_t index) {
    ListWindow::OnEntryContextMenu(index);
    this->ShowContextMenu();
    return true;
}

void TrackListView::ShowContextMenu() {
    const auto row = this->GetSelectedTrackIndex();
    if (row == ListWindow::NO_SELECTION) return;

    if (headers.HeaderAt(this->GetSelectedIndex())) {
        PlayQueueOverlays::ShowAlbumDividerOverlay(
            MessageQueue(), this->playback, this->library, this->GetSelectedTrack(), this->addCategoryToPlaylist);
    }
    else if (!this->activateQueueRow) {
        auto track = this->resultWindow->Result().Track(row);
        if (track) PlayQueueOverlays::ShowAddTrackOverlay(
            MessageQueue(), this->library, this->playback, {},
            std::make_shared<const musik::core::api::TrackRef>(std::move(track)), this->addTrackToPlaylist);
    }
    else if (auto track = this->GetSelectedTrack()) {
        PlayQueueOverlays::ShowAddTrackOverlay(
            MessageQueue(), this->library, this->playback, track, {}, this->addTrackToPlaylist);
    }
}

bool TrackListView::KeyPress(const std::string& key) {
    bool handled = false;

    if (Hotkeys::Is(Hotkeys::ContextMenu, key)) {
        this->ShowContextMenu();
        handled = true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateJumpToPlaying, key)) {
        this->ScrollToPlaying();
        handled = true;
    }
    else if (Hotkeys::Is(Hotkeys::PlayQueueHotSwap, key)) {
        if (!headers.HeaderAt(this->GetSelectedIndex())) {
            /* A queue-backed view already displays the playback queue. */
            const bool swapped = this->activateQueueRow ? this->GetSelectedTrackIndex() != ListWindow::NO_SELECTION
                : playback::HotSwap(*this, this->playback);
            if (swapped) {
                ToastOverlay::Show(_TSTR("tracklist_hot_swap_success_toast"), 2000);
            }
        }
        handled = true;
    }
    else if (Hotkeys::Is(Hotkeys::TrackListRateTrack, key)) {
        TrackPtr track = this->GetSelectedTrack();
        if (track && !headers.HeaderAt(this->GetSelectedIndex())) {
            TrackOverlays::ShowRateTrackOverlay(
                track,
                this->library,
                [this](bool success) {
                    if (success && this->resultWindow) {
                        this->InvalidateData();
                    }
                });
        }
        handled = true;
    }
    else if (Hotkeys::Is(Hotkeys::TrackListNextGroup, key)) {
        const size_t next = this->headers.NextHeaderIndex(this->GetSelectedIndex());
        if (next != HeaderCalculator::NO_INDEX) {
            this->SetSelectedIndex(next);
            if (!IsEntryVisible(next)) {
                this->ScrollTo(next);
            }
        }
        handled = true;
    }
    else if (Hotkeys::Is(Hotkeys::TrackListPreviousGroup, key)) {
        const size_t prev = this->headers.PrevHeaderIndex(this->GetSelectedIndex());
        if (prev != HeaderCalculator::NO_INDEX) {
            this->SetSelectedIndex(prev);
            if (!IsEntryVisible(prev)) {
                this->ScrollTo(prev);
            }
        }
        handled = true;
    }
    else if (Hotkeys::Is(Hotkeys::TrackListPlayFromTop, key)) {
        this->PlayRow(0);
        handled = true;
    }

    if (!handled) {
        handled = ListWindow::KeyPress(key);
    }

    if (handled) {
        this->lastChanged = now();

        this->Debounce(
            ScrollToPlayingMessage,
            0, 0, AUTO_SCROLL_COOLDOWN.count());
    }

    return handled;
}

void TrackListView::OnTrackChanged(size_t index, musik::core::TrackPtr track) {
    this->playing = track;
    this->OnAdapterChanged();

    if (now() - lastChanged >= AUTO_SCROLL_COOLDOWN) {
        this->ScrollToPlaying();
    }
}

IScrollAdapter& TrackListView::GetScrollAdapter() noexcept {
    return *this->adapter;
}

TrackListView::Adapter::Adapter(TrackListView &parent)
: parent(parent) {
}

/* * * * TrackListView::HeaderCalculator * * * */

void TrackListView::HeaderCalculator::Set(Headers rawOffsets, Durations durations) {
    this->rawOffsets = rawOffsets;
    this->durations = durations;
    this->absoluteOffsets.reset();
    if (rawOffsets) {
        this->absoluteOffsets = std::make_shared<std::set<size_t>>();
        size_t i = 0;
        for (auto val : (*this->rawOffsets)) {
            this->absoluteOffsets->insert(val + i);
            i++;
        }
    }
}

void TrackListView::HeaderCalculator::Reset() noexcept {
    this->absoluteOffsets.reset();
    this->rawOffsets.reset();
}

size_t TrackListView::HeaderCalculator::AdapterToTrackListIndex(size_t index) {
    return this->ApplyHeaderOffset(index, this->absoluteOffsets, -1);
}

size_t TrackListView::HeaderCalculator::DurationFromAdapterIndex(size_t index) {
    /* ugh, HeaderCalculator should probably be re-thought, but this is especially ugly */
    const auto adjustedIndex = this->ApplyHeaderOffset(index + 1, this->absoluteOffsets, -1);
    const auto it = this->durations->find(adjustedIndex);
    if (it != this->durations->end()) {
        return it->second;
    }
    return 0;
}

size_t TrackListView::HeaderCalculator::TrackListToAdapterIndex(size_t index) {
    return this->ApplyHeaderOffset(index, this->rawOffsets, 1);
}

size_t TrackListView::HeaderCalculator::ApplyHeaderOffset(size_t index, Headers offsets, int delta) {
    size_t result = index;
    if (offsets) {
        for (auto offset : (*offsets)) {
            if (offset <= index) {
                result += delta;
            }
            else {
                break;
            }
        }
    }
    return result;
}

size_t TrackListView::HeaderCalculator::Count() noexcept {
    return this->absoluteOffsets ? this->absoluteOffsets->size() : 0;
}

bool TrackListView::HeaderCalculator::HeaderAt(size_t index) {
    return this->absoluteOffsets &&
        this->absoluteOffsets->find(index) != this->absoluteOffsets->end();
}

size_t TrackListView::HeaderCalculator::NextHeaderIndex(size_t selectedIndex) noexcept {
    size_t result = NO_INDEX;
    if (this->absoluteOffsets) {
        for (auto val : (*this->absoluteOffsets)) {
            if (val > selectedIndex) {
                result = val;
                break;
            }
        }
    }
    return result;
}

size_t TrackListView::HeaderCalculator::PrevHeaderIndex(size_t selectedIndex) noexcept {
    size_t result = 0;
    if (this->absoluteOffsets) {
        for (auto val : (*this->absoluteOffsets)) {
            if (selectedIndex > val) {
                result = val;
            }
            else {
                break;
            }
        }
    }
    return result;
}

/* * * * TrackListView::Adapter * * * */

size_t TrackListView::Adapter::GetEntryCount() noexcept {
    return parent.TrackCount() + parent.headers.Count();
}

void TrackListView::Adapter::DrawPage(ScrollableWindow* window, size_t index, ScrollPosition& result) {
    ScrollAdapterBase::DrawPage(window, index, result);
    if (parent.resultWindow && result.visibleEntryCount) {
        const auto first = result.firstVisibleEntryIndex;
        const auto update = parent.resultWindow->SetViewport(parent.RowIndex(first),
            parent.RowIndex(first + result.visibleEntryCount - 1),
            static_cast<size_t>(std::max(1, parent.GetContentHeight())));
        if (update == api::TrackResultWindow::ViewportUpdate::Changed) {
            parent.UpdatePagePolling(parent.IsParentVisible());
            parent.Post(LoadTrackPageMessage);
        }
    }
}

IScrollAdapter::EntryPtr TrackListView::Adapter::GetEntry(cursespp::ScrollableWindow* window, size_t rawIndex) {
    const bool selected = (rawIndex == parent.GetSelectedIndex());

    if (this->parent.headers.HeaderAt(rawIndex)) {
        /* the next track at the next logical index will have the album
        tracks we're interesetd in. */
        auto const trackIndex = this->parent.headers.AdapterToTrackListIndex(rawIndex + 1);
        TrackPtr track = parent.DisplayTrack(trackIndex);

        if (track) {
            std::string album = track->GetMetadataState() == MetadataState::Loaded
                ? track->GetString(constants::Track::ALBUM) : "-";

            if (!album.size()) {
                album = _TSTR("tracklist_unknown_album");
            }

            const auto duration = this->parent.headers.DurationFromAdapterIndex(rawIndex);
            if (duration > 0) {
                album += " - " + core::duration::DurationWithHours(duration);
            }

            album = text::Ellipsize(album, this->GetWidth());

            auto entry = std::make_shared<TrackListEntry>(
                album, narrow_cast<int>(trackIndex), RowType::Separator);

            entry->SetAttrs(selected
                ? Color::ListItemHeaderHighlighted
                : Color::ListItemHeader);

            return entry;
        }
    }

    const size_t trackIndex = this->parent.headers.AdapterToTrackListIndex(rawIndex);
    TrackPtr track = parent.DisplayTrack(trackIndex);

    Color attrs = Color::Default;

    if (parent.decorator) {
        attrs = parent.decorator(track, trackIndex);
    }
    else {
        attrs = selected ? Color::ListItemHighlighted : Color::Default;

        TrackPtr playing = parent.playing;
        if (playing && track &&
            playing->GetId() == track->GetId() &&
            playing->LibraryId() == track->LibraryId())
        {
            if (selected) {
                attrs = Color::ListItemHighlightedSelected;
            }
            else {
                attrs = Color::ListItemSelected;
            }
        }
    }

    if (!track) {
        auto entry = std::make_shared<SingleLineEntry>("  -");
        entry->SetAttrs(attrs);
        return entry;
    }

    if (track->GetMetadataState() != MetadataState::Loaded) {
        attrs = Color::TextDisabled;
    }

    std::string text = parent.renderer(
        track, rawIndex, this->GetWidth(), parent.trackNumType);

    auto entry = std::make_shared<TrackListEntry>(
        text, narrow_cast<int>(trackIndex), RowType::Track);

    entry->SetAttrs(attrs);

    return entry;
}
