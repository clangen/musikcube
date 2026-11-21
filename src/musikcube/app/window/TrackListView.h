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

#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/Colors.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/IKeyHandler.h>
#include <cursespp/ListWindow.h>
#include <cursespp/SingleLineEntry.h>


#include <musikcore/runtime/IMessage.h>

#include <app/util/TrackRowRenderers.h>
#include <app/util/AsyncCallbackScope.h>
#include <musikcore/track-result-window.hpp>
#include <musikcore/playback.hpp>
#include <map>
#include <set>

namespace musik {
    namespace cube {
        class TrackListView:
            public cursespp::ListWindow,
            public sigslot::has_slots<>
        {
            enum MessageType {
                ScrollToPlayingMessage = 1003,
                LoadTrackPageMessage = 1004,
                RetryTrackRequestMessage = 1005
            };

            public:
                /* types */
                using RowDecorator = std::function<cursespp::Color(size_t)>;
                using Headers = std::shared_ptr<std::set<size_t>>;
                using Durations = std::shared_ptr<std::map<size_t, size_t>>;

                /* ctor, dtor */
                TrackListView(
                    musik::core::api::Context& context,
                    RowDecorator decorator = RowDecorator());
                ~TrackListView() override;
                void OnParentVisibilityChanged(bool visible) override;

                /* IWindow */
                void ProcessMessage(musik::core::runtime::IMessage &message) override;
                bool KeyPress(const std::string& key) override;

                /* regular methods */
                std::vector<int64_t> TrackIds() const;
                bool HasTrackResult() const noexcept { return bool(this->resultWindow); }
                using AddTrackToPlaylist = std::function<void(int64_t)>;
                using AddCategoryToPlaylist = std::function<void(const std::string&, const std::string&, int64_t)>;
                void SetAddToPlaylistCallbacks(AddTrackToPlaylist track, AddCategoryToPlaylist category) {
                    this->addTrackToPlaylist = std::move(track);
                    this->addCategoryToPlaylist = std::move(category);
                }
                void SetQueueActivationCallback(std::function<void(size_t)> callback) {
                    this->activateQueueRow = std::move(callback);
                }
                size_t GetSelectedTrackIndex();
                size_t TrackIndexToAdapterIndex(size_t index);
                size_t TrackCount() noexcept;
                size_t EntryCount();
                void InvalidateData();

                void SetTrackNumType(TrackRowRenderers::TrackNumType type);
                void SetRowRenderer(TrackRowRenderers::Renderer renderer);

                void Reset();
                using TrackRequest = std::function<mc_request_id(musik::core::api::Context&,
                    musik::core::api::TrackSearchCompletion)>;
                void RequeryTracks(size_t hash, TrackRequest request);
                void SetTrackResult(musik::core::api::TrackResult result, size_t hash);
                void SetTrackIds(const int64_t* ids, size_t count, size_t hash);
                void SetEditedTrackIds(const std::vector<int64_t>& ids) {
                    this->CancelTrackRequest();
                    this->SetTrackIds(ids.data(), ids.size(), this->lastQueryHash);
                }
                void CancelTrackRequests();
                bool PlayRow(size_t row);

            protected:
                /* IScrollableWindow */
                cursespp::IScrollAdapter& GetScrollAdapter() noexcept override;
                bool OnEntryActivated(size_t index) override;
                bool OnEntryContextMenu(size_t index) override;
                void ShowContextMenu();
                void ShowRatingOverlay();
                void OnSelectionChanged(size_t newIndex, size_t oldIndex) override;
                void OnFocusChanged(bool focused) override;
                void OnVisibilityChanged(bool visible) override;

                /* this view has headers and track entry types */
                enum class RowType : char { Track = 't', Separator = 's' };

                /* our special type of list entry */
                class TrackListEntry : public cursespp::SingleLineEntry {
                    public:
                        TrackListEntry(const std::string& str, int index, RowType type)
                            : cursespp::SingleLineEntry(str), index(index), type(type) { }

                        RowType GetType() noexcept { return type; }
                        int GetIndex() noexcept { return index; }

                    private:
                        RowType type;
                        int index;
                };

                /* our adapter */
                class Adapter : public cursespp::ScrollAdapterBase {
                    public:
                        Adapter(TrackListView &parent);
                        size_t GetEntryCount() noexcept override;
                        EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) override;
                        void DrawPage(cursespp::ScrollableWindow* window, size_t index, ScrollPosition& result) override;

                    private:
                        TrackListView &parent;
                        IScrollAdapter::ScrollPosition spos;
                };

            private:
                void ApplyQueryResult(Headers headers, Durations durations, size_t hash, bool hadTracks);
                size_t RowIndex(size_t adapterIndex);
                void LoadTrackPage();
                void UpdatePagePolling(bool pending);
                void HandlePageStatus(mc_result status);
                void CancelTrackRequest();
                void SubmitTrackRequest();
                musik::core::api::Context& musikcore;
                std::unique_ptr<musik::core::api::TrackResultWindow> resultWindow;
                AsyncCallbackScope trackQueryWork;
                AsyncCallbackScope albumActionWork;
                AsyncCallbackScope ratingDialogWork;
                mc_request_id trackRequest = 0;
                TrackRequest pendingTrackQuery;
                size_t pendingTrackHash = 0;
                bool pollingPages = false;
                AddTrackToPlaylist addTrackToPlaylist;
                AddCategoryToPlaylist addCategoryToPlaylist;
                std::function<void(size_t)> activateQueueRow;
                /* class to help with header offset calculation. this thing is really gross and
                should probably be refactored at some point. */
                class HeaderCalculator {
                    public:
                        static const size_t NO_INDEX = (size_t) -1;

                        void Set(Headers rawOffsets, Durations durations);
                        void Reset() noexcept;
                        bool HeaderAt(size_t index);
                        size_t AdapterToTrackListIndex(size_t index);
                        size_t TrackListToAdapterIndex(size_t index);
                        size_t DurationFromAdapterIndex(size_t index);
                        size_t NextHeaderIndex(size_t selectedIndex) noexcept;
                        size_t PrevHeaderIndex(size_t selectedIndex) noexcept;
                        size_t Count() noexcept;

                    private:
                        size_t ApplyHeaderOffset(size_t index, Headers offsets, int delta);

                        Headers absoluteOffsets;
                        Headers rawOffsets;
                        Durations durations;
                };

                void RefreshPlayingTrack();

                void ScrollToPlaying();
                void SelectFirstTrack();

                HeaderCalculator headers;
                std::unique_ptr<Adapter> adapter;
                musik::core::api::Playback apiPlayback;
                std::optional<int64_t> playingId;
                size_t lastQueryHash;
                RowDecorator decorator;
                TrackRowRenderers::Renderer renderer;
                std::chrono::milliseconds lastChanged;
                TrackRowRenderers::TrackNumType trackNumType;
        };
    }
}
