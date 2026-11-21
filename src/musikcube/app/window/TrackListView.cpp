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

#include <musikcore/library.h>
#include "TrackListView.h"

#include <cursespp/Colors.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>
#include <cursespp/ToastOverlay.h>

#include <common/Duration.h>

#include <app/util/Hotkeys.h>
#include <app/overlay/PlayQueueOverlays.h>
#include <app/overlay/TrackOverlays.h>

static constexpr int kTrackPageRetryMs = 25;
static constexpr int kRemoteSkeletonDelayMs = 50;

using namespace musik::core;
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
    api::Context& context,
    RowDecorator decorator)
: ListWindow(nullptr)
, musikcore(context)
, trackQuery(context,
    /* Schedules a retry for a temporarily unavailable track query. */
    [this] { this->Debounce(RetryTrackRequestMessage, 0, 0, kTrackPageRetryMs); },

    /* Receives the selected source's result or its terminal error. */
    [this](const TrackQuery& query, mc_result status, api::TrackResult result, std::string_view error) {
        if (status == MC_OK) {
            this->InstallTrackResult(std::move(result), query.source);
        }
        else if (status != MC_ERROR_CANCELLED) {
            api::WriteLog(MC_LOG_ERROR, "TrackListView", error);
        }
    })
, apiPlayback(this->musikcore.GetPlayback())
{
    this->adapter = std::make_unique<Adapter>(*this);
    this->lastChanged = now();
    this->decorator = decorator;
    this->trackNumType = TrackRowRenderers::TrackNumType::Metadata;
    this->renderer = TrackRowRenderers::Get(TrackRowRenderers::Type::AlbumSort);
    this->apiPlayback.SetQueueChangedCallback([](void* data) {
        static_cast<TrackListView*>(data)->RefreshPlayingTrack();
    }, this);
    if (const auto playing = this->apiPlayback.CurrentTrack()) {
        this->playingId = playing->Id();
    }
}

TrackListView::~TrackListView() {
    this->apiPlayback.SetQueueChangedCallback(nullptr);
    this->CancelTrackRequests();
}

void TrackListView::CancelTrackRequest() {
    this->Remove(RetryTrackRequestMessage);
    this->trackQuery.Clear();
    this->resultActionWork.Invalidate();
}

void TrackListView::CancelTrackRequests() {
    this->albumActionWork.Invalidate();
    this->CancelTrackRequest();
    if (this->resultWindow) this->resultWindow->Clear();
    this->UpdatePagePolling(false);
}

mc_request_id TrackListView::TrackQuery::Open(api::Context& context, api::TrackSearchCompletion completion) const {
    return std::visit([&](const auto& source) {
        return source.Open(context, this->library, std::move(completion));
    }, this->source);
}

void TrackListView::Show(TrackSource source) {
    if (const auto* pending = this->trackQuery.Pending()) {
        if (pending->source == source) {
            return;
        }
    }
    else if (const auto* previous = std::get_if<TrackSource>(&this->lastQuery)) {
        if (this->resultWindow && *previous == source) {
            return;
        }
    }
    this->Refresh(std::move(source));
}

void TrackListView::Refresh(TrackSource source) {
    const auto* previous = std::get_if<TrackSource>(&this->lastQuery);
    if (!previous || *previous != source) {
        this->Reset();
    }
    else {
        this->CancelTrackRequest();
    }

    /* Keep retries bound to the catalog selected when the query was made. */
    try {
        if (auto library = this->musikcore.GetLibrary()) {
            this->trackQuery.Replace({ std::move(source), std::move(*library) });
        }
    }
    catch (const api::Error& error) {
        api::WriteLog(MC_LOG_ERROR, "TrackListView", error.what());
    }
}

void TrackListView::SetEditedTrackIds(const std::vector<int64_t>& ids) {
    this->CancelTrackRequest();
    if (!this->TrackCount()) {
        this->Reset();
        return;
    }
    const auto origin = this->resultWindow->Result().Track(0).Origin();
    this->InstallTrackResult(api::MakeTrackResult(origin, ids.data(), ids.size()), this->lastQuery);
}

void TrackListView::SetTrackResult(api::TrackResult result) {
    this->CancelTrackRequest();
    this->InstallTrackResult(std::move(result), std::monostate {});
}

void TrackListView::SetSearchResult(api::TrackResult result, std::string filter) {
    this->CancelTrackRequest();
    this->InstallTrackResult(std::move(result), std::move(filter));
}

void TrackListView::InstallTrackResult(api::TrackResult result, QueryKey key) {
    this->ratingDialogWork.Invalidate();
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
    this->remoteRows = this->musikcore.GetLibraryStatus().kind == MC_LIBRARY_REMOTE;
    this->DelayRemoteSkeleton();
    this->resultWindow = std::make_unique<api::TrackResultWindow>(this->musikcore,
        std::move(result), std::vector<std::string>{ MC_TRACK_FIELD_ID, MC_TRACK_FIELD_TITLE, MC_TRACK_FIELD_ARTIST,
            MC_TRACK_FIELD_ALBUM, MC_TRACK_FIELD_DURATION, MC_TRACK_FIELD_TRACK, MC_TRACK_FIELD_RATING },
        [this](size_t offset, size_t count, mc_result status) {
            this->UpdatePagePolling(this->resultWindow->Pending() && this->IsParentVisible());
            if (this->resultWindow->IntersectsViewport(offset, count)) this->Redraw();
            this->HandlePageStatus(status);
        });

    this->ApplyQueryResult(std::move(headers), std::move(durations), std::move(key), hadTracks);
}

void TrackListView::OnParentVisibilityChanged(bool visible) {
    ListWindow::OnParentVisibilityChanged(visible);
    if (!visible) {
        this->ratingDialogWork.Invalidate();
        this->UpdatePagePolling(false);
    }
    else if (this->resultWindow) {
        this->Post(LoadTrackPageMessage);
    }
}

void TrackListView::OnSelectionChanged(size_t newIndex, size_t oldIndex) {
    ListWindow::OnSelectionChanged(newIndex, oldIndex);
    this->ratingDialogWork.Invalidate();
}

void TrackListView::OnFocusChanged(bool focused) {
    ListWindow::OnFocusChanged(focused);
    if (!focused) {
        this->ratingDialogWork.Invalidate();
    }
}

void TrackListView::OnVisibilityChanged(bool visible) {
    ListWindow::OnVisibilityChanged(visible);
    if (!visible) {
        this->ratingDialogWork.Invalidate();
        this->UpdatePagePolling(false);
    }
    else if (this->resultWindow) {
        this->Post(LoadTrackPageMessage);
    }
}

void TrackListView::UpdatePagePolling(bool pending) {
    App::Instance().SetInputPolling(*this, pending && this->IsVisible() && this->IsParentVisible());
}

void TrackListView::HandlePageStatus(mc_result status) {
    if (status == MC_ERROR_BUSY || status == MC_ERROR_QUEUE_FULL) this->Debounce(LoadTrackPageMessage, 0, 0, kTrackPageRetryMs);
    else if (status != MC_OK && status != MC_ERROR_CANCELLED) {
        musik::core::api::WriteLog(MC_LOG_ERROR, "TrackListView", api::Error(status).what());
    }
}

void TrackListView::DelayRemoteSkeleton() {
    if (this->remoteRows) {
        /* Fetch immediately and leave short remote misses blank. */
        this->skeletonAfter = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(kRemoteSkeletonDelayMs);
        this->Debounce(ShowTrackSkeletonMessage, 0, 0, kRemoteSkeletonDelayMs);
    }
    else {
        this->Remove(ShowTrackSkeletonMessage);
        this->skeletonAfter = {};
    }
}

void TrackListView::LoadTrackPage() {
    if (!this->resultWindow || !this->IsVisible() || !this->IsParentVisible()) return;
    const auto status = this->resultWindow->Load();
    this->UpdatePagePolling(this->resultWindow->Pending());
    this->HandlePageStatus(status);
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

void TrackListView::ApplyQueryResult(Headers headers, Durations durations, QueryKey key, bool hadTracks) {
    const bool preserveSelection = hadTracks && this->lastQuery == key;
    this->headers.Set(std::move(headers), std::move(durations));
    this->lastQuery = std::move(key);
    this->OnAdapterChanged();

    /* A refresh of the same query keeps the user's position. */
    if (!preserveSelection) {
        this->ScrollToTop();
        this->SelectFirstTrack();
    }
}

std::vector<int64_t> TrackListView::TrackIds() const {
    std::vector<int64_t> ids;
    if (this->resultWindow) {
        const auto& result = this->resultWindow->Result();
        ids.reserve(result.Size());
        for (size_t row = 0; row < result.Size(); ++row) {
            ids.push_back(result.Id(row));
        }
    }
    return ids;
}

void TrackListView::Reset() {
    this->CancelTrackRequest();
    this->UpdatePagePolling(false);
    this->resultWindow.reset();
    this->Remove(ShowTrackSkeletonMessage);
    this->headers.Reset();
    this->OnAdapterChanged();
}

void TrackListView::InvalidateData() {
    if (this->resultWindow) {
        this->DelayRemoteSkeleton();
        this->resultWindow->Clear();
        this->Post(LoadTrackPageMessage);
    }
    this->OnAdapterChanged();
}

size_t TrackListView::GetSelectedTrackIndex() {
    const auto i = this->GetSelectedIndex();
    if (i == ListWindow::NO_SELECTION || i >= this->EntryCount()) return ListWindow::NO_SELECTION;
    return this->RowIndex(i);
}

size_t TrackListView::RowIndex(size_t adapterIndex) {
    return this->headers.AdapterToTrackListIndex(adapterIndex + this->headers.HeaderAt(adapterIndex));
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
    if (this->playingId && this->TrackCount()) {
        const int64_t id = *this->playingId;
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
    if (message.Type() == ShowTrackSkeletonMessage) {
        if (this->resultWindow && this->resultWindow->Pending() && this->IsParentVisible()) {
            this->Redraw();
        }
        return;
    }
    if (message.Type() == LoadTrackPageMessage) {
        this->LoadTrackPage();
    }
    else if (message.Type() == RetryTrackRequestMessage) {
        this->trackQuery.TrySubmit();
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
        this->ShowContextMenu();
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
    this->ratingDialogWork.Invalidate();
    const auto row = this->GetSelectedTrackIndex();
    if (row == ListWindow::NO_SELECTION) return;
    const auto token = this->resultActionWork.CurrentToken();
    if (headers.HeaderAt(this->GetSelectedIndex())) {
        /* A confirmed album action can finish after the user opens the queue view. */
        PlayQueueOverlays::ShowAlbumDividerOverlay(
            MessageQueue(), this->musikcore, this->albumActionWork.CurrentToken(),
            this->resultWindow->Result().Track(row), this->addCategoryToPlaylist);
    }
    else {
        PlayQueueOverlays::ShowAddTrackOverlay(MessageQueue(), this->musikcore, token,
            this->resultWindow->Result().Track(row), !this->activateQueueRow, this->addTrackToPlaylist);
    }
}

void TrackListView::ShowRatingOverlay() {
    const auto selectedIndex = this->GetSelectedIndex();
    const auto row = this->GetSelectedTrackIndex();
    if (row == ListWindow::NO_SELECTION || this->headers.HeaderAt(selectedIndex)) {
        return;
    }

    auto track = std::make_shared<api::TrackRef>(this->resultWindow->Result().Track(row));
    const auto token = this->resultActionWork.CurrentToken();
    const auto dialogToken = this->ratingDialogWork.Begin();
    try {
        api::ReadTrack(this->musikcore, *track, { MC_TRACK_FIELD_RATING },
            this->resultActionWork.Wrap(
                [this, track, token, dialogToken, selectedIndex]
                (mc_result status, api::TrackPage page, std::string_view) {
                    /* A late read must not interrupt another view or modal. */
                    if (status != MC_OK || !page.Size() || !dialogToken.IsCurrent() ||
                        this->GetSelectedIndex() != selectedIndex ||
                        !this->IsFocused() || !this->IsVisible() || !this->IsParentVisible() || App::Overlays().Top())
                    {
                        return;
                    }

                    const auto rating = static_cast<int>(page.Track(0).Rating().value_or(0));
                    TrackOverlays::ShowRateTrackOverlay(rating, [this, track, token](uint32_t value) {
                        if (!token.IsCurrent()) {
                            return;
                        }
                        try {
                            api::SetTrackRating(this->musikcore, *track, value,
                                this->resultActionWork.Wrap([this](mc_result status, std::string_view) {
                                    if (status == MC_OK) {
                                        this->InvalidateData();
                                    }
                                }));
                        }
                        catch (const api::Error& error) {
                            musik::core::api::WriteLog(MC_LOG_ERROR, "TrackListView", error.what());
                        }
                    });
                }));
    }
    catch (const api::Error& error) {
        musik::core::api::WriteLog(MC_LOG_ERROR, "TrackListView", error.what());
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
            const auto row = this->IsFocused() ? this->GetSelectedTrackIndex() : 0;
            const bool swapped = this->activateQueueRow
                ? this->GetSelectedTrackIndex() != ListWindow::NO_SELECTION
                : this->resultWindow && this->apiPlayback.HotSwapQueue(this->resultWindow->Result(), row) == MC_OK;
            if (swapped) {
                ToastOverlay::Show(_TSTR("tracklist_hot_swap_success_toast"), 2000);
            }
        }
        handled = true;
    }
    else if (Hotkeys::Is(Hotkeys::TrackListRateTrack, key)) {
        this->ShowRatingOverlay();
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

void TrackListView::RefreshPlayingTrack() {
    try {
        const auto track = this->apiPlayback.CurrentTrack();
        const auto id = track ? std::optional<int64_t>(track->Id()) : std::nullopt;
        if (id == this->playingId) {
            return;
        }
        this->playingId = id;
    }
    catch (const api::Error& error) {
        musik::core::api::WriteLog(MC_LOG_ERROR, "TrackListView", error.what());
        return;
    }

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
    const int height = parent.GetContentHeight();
    if (parent.remoteRows && (parent.skeletonScrollIndex != index || parent.skeletonViewportHeight != height)) {
        parent.skeletonScrollIndex = index;
        parent.skeletonViewportHeight = height;
        parent.DelayRemoteSkeleton();
    }

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
    const bool showSkeleton = !parent.remoteRows || std::chrono::steady_clock::now() >= parent.skeletonAfter;

    if (this->parent.headers.HeaderAt(rawIndex)) {
        /* the next track at the next logical index will have the album
        tracks we're interesetd in. */
        auto const trackIndex = this->parent.headers.AdapterToTrackListIndex(rawIndex + 1);
        if (trackIndex < parent.TrackCount()) {
            const auto track = parent.resultWindow->CachedRow(trackIndex);
            std::string album = track && track->Value(MC_TRACK_FIELD_ID).data()
                ? std::string(track->Album())
                : "-";

            if ((!track || !track->Value(MC_TRACK_FIELD_ID).data()) && !showSkeleton) {
                return std::make_shared<SingleLineEntry>("");
            }

            if (!album.size()) {
                album = _TSTR("tracklist_unknown_album");
            }

            const auto duration = this->parent.headers.DurationFromAdapterIndex(rawIndex);
            if (duration > 0) {
                album += " - " + musik::duration::DurationWithHours(duration);
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
    if (trackIndex >= parent.TrackCount()) {
        auto entry = std::make_shared<SingleLineEntry>("  -");
        entry->SetAttrs(selected ? Color::ListItemHighlighted : Color::Default);
        return entry;
    }

    const auto id = parent.resultWindow->Result().Id(trackIndex);
    const auto track = parent.resultWindow->CachedRow(trackIndex);
    const bool loaded = track && track->Value(MC_TRACK_FIELD_ID).data();
    Color attrs = Color::Default;

    if (parent.decorator) {
        attrs = parent.decorator(trackIndex);
    }
    else {
        attrs = selected ? Color::ListItemHighlighted : Color::Default;

        if (parent.playingId == id)
        {
            if (selected) {
                attrs = Color::ListItemHighlightedSelected;
            }
            else {
                attrs = Color::ListItemSelected;
            }
        }
    }

    if (!loaded) {
        attrs = Color::TextDisabled;
    }

    std::string text;
    if (loaded || showSkeleton) {
        text = parent.renderer(loaded ? &*track : nullptr, id, rawIndex, this->GetWidth(), parent.trackNumType);
    }

    auto entry = std::make_shared<TrackListEntry>(
        text, narrow_cast<int>(trackIndex), RowType::Track);

    entry->SetAttrs(attrs);

    return entry;
}
