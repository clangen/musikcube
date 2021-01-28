//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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

#include <musikcore/library/query/TrackListQueryBase.h>
#include <musikcore/audio/PlaybackService.h>

#include <musikcore/runtime/IMessage.h>
#include <musikcore/library/ILibrary.h>

#include <app/util/TrackRowRenderers.h>

namespace musik {
    namespace cube {
        class TrackListView:
            public cursespp::ListWindow,
            public sigslot::has_slots<>
        {
            public:
                typedef musik::core::TrackPtr TrackPtr;
                typedef musik::core::library::query::TrackListQueryBase TrackListQueryBase;

                /* events */
                sigslot::signal1<musik::core::library::query::TrackListQueryBase*> Requeried;

                /* types */
                using RowDecorator = std::function<cursespp::Color(TrackPtr, size_t)>;
                using Headers = TrackListQueryBase::Headers;
                using Durations = TrackListQueryBase::Durations;

                /* ctor, dtor */
                TrackListView(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    RowDecorator decorator = RowDecorator());

                /* IWindow */
                void ProcessMessage(musik::core::runtime::IMessage &message) override;
                bool KeyPress(const std::string& key) override;

                /* regular methods */
                std::shared_ptr<musik::core::TrackList> GetTrackList() noexcept;
                void SetTrackList(std::shared_ptr<musik::core::TrackList> trackList);
                musik::core::TrackPtr GetSelectedTrack();
                size_t GetSelectedTrackIndex();
                size_t TrackIndexToAdapterIndex(size_t index);
                size_t TrackCount() noexcept;
                size_t EntryCount();
                void InvalidateData();

                void SetTrackNumType(TrackRowRenderers::TrackNumType type);
                void SetRowRenderer(TrackRowRenderers::Renderer renderer);

                void Reset();
                void Requery(std::shared_ptr<TrackListQueryBase> query);

            protected:
                /* IScrollableWindow */
                cursespp::IScrollAdapter& GetScrollAdapter() noexcept override;
                bool OnEntryActivated(size_t index) override;
                bool OnEntryContextMenu(size_t index) override;
                void OnDimensionsChanged() override;

                void OnTrackListWindowCached(
                    const musik::core::TrackList* track, size_t from, size_t to);

                void SetTrackListAndUpateEventHandlers(
                    std::shared_ptr<musik::core::TrackList> trackList);

                void OnQueryCompleted(musik::core::db::IQuery* query);
                void ShowContextMenu();

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
                        size_t GetEntryCount() override;
                        EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) override;

                    private:
                        TrackListView &parent;
                        IScrollAdapter::ScrollPosition spos;
                };

            private:
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

                void OnTrackChanged(size_t index, musik::core::TrackPtr track);

                void AdjustTrackListCacheWindowSize();

                void ScrollToPlaying();
                void SelectFirstTrack();

                std::shared_ptr<TrackListQueryBase> query;
                std::shared_ptr<musik::core::TrackList> tracks;
                HeaderCalculator headers;
                std::unique_ptr<Adapter> adapter;
                musik::core::audio::PlaybackService& playback;
                musik::core::TrackPtr playing;
                musik::core::ILibraryPtr library;
                size_t lastQueryHash;
                RowDecorator decorator;
                TrackRowRenderers::Renderer renderer;
                std::chrono::milliseconds lastChanged;
                TrackRowRenderers::TrackNumType trackNumType;
        };
    }
}
