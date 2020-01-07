//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <core/library/query/local/TrackListQueryBase.h>
#include <core/audio/PlaybackService.h>

#include <core/runtime/IMessage.h>
#include <core/library/ILibrary.h>

#include <app/util/TrackRowRenderers.h>

namespace musik {
    namespace cube {
        class TrackListView:
            public cursespp::ListWindow,
            public sigslot::has_slots<>
        {
            public:
                typedef musik::core::TrackPtr TrackPtr;
                typedef musik::core::db::local::TrackListQueryBase TrackListQueryBase;

                /* events */
                sigslot::signal1<musik::core::db::local::TrackListQueryBase*> Requeried;

                /* types */
                typedef std::function<cursespp::Color(TrackPtr, size_t)> RowDecorator;
                typedef std::shared_ptr<std::set<size_t> > Headers;

                /* ctor, dtor */
                TrackListView(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    RowDecorator decorator = RowDecorator());

                virtual ~TrackListView();

                /* IWindow */
                virtual void ProcessMessage(musik::core::runtime::IMessage &message);
                virtual bool KeyPress(const std::string& key);

                /* regular methods */
                std::shared_ptr<musik::core::TrackList> GetTrackList();
                void SetTrackList(std::shared_ptr<musik::core::TrackList> trackList);
                musik::core::TrackPtr GetSelectedTrack();
                size_t GetSelectedTrackIndex();
                size_t TrackIndexToAdapterIndex(size_t index);
                void Clear();
                size_t TrackCount();
                size_t EntryCount();
                void InvalidateData();

                void SetTrackNumType(TrackRowRenderers::TrackNumType type);
                void SetRowRenderer(TrackRowRenderers::Renderer renderer);

                void Requery(std::shared_ptr<TrackListQueryBase> query);

            protected:
                virtual cursespp::IScrollAdapter& GetScrollAdapter();

                virtual void OnEntryActivated(size_t index);
                virtual void OnEntryContextMenu(size_t index);

                void OnQueryCompleted(musik::core::db::IQuery* query);
                void ShowContextMenu();

                /* this view has headers and track entry types */
                enum class RowType : char { Track = 't', Separator = 's' };

                /* our special type of list entry */
                class TrackListEntry : public cursespp::SingleLineEntry {
                    public:
                        TrackListEntry(const std::string& str, int index, RowType type)
                            : cursespp::SingleLineEntry(str), index(index), type(type) { }

                        virtual ~TrackListEntry() { }

                        RowType GetType() { return type; }
                        int GetIndex() { return index; }

                    private:
                        RowType type;
                        int index;
                };

                /* our adapter */
                class Adapter : public cursespp::ScrollAdapterBase {
                    public:
                        Adapter(TrackListView &parent);
                        virtual ~Adapter() { }

                        virtual size_t GetEntryCount();
                        virtual EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index);

                    private:
                        TrackListView &parent;
                        IScrollAdapter::ScrollPosition spos;
                };

            private:
                /* class to help with header offset calculation */
                class HeaderCalculator {
                    public:
                        void Set(Headers rawOffsets);
                        void Reset();
                        size_t AdapterToTrackListIndex(size_t index);
                        size_t TrackListToAdapterIndex(size_t index);
                        bool HeaderAt(size_t index);
                        size_t Count();

                    private:
                        size_t ApplyHeaderOffset(size_t index, Headers offsets, int delta);

                        Headers absoluteOffsets;
                        Headers rawOffsets;
                };

                void OnTrackChanged(size_t index, musik::core::TrackPtr track);

                void ScrollToPlaying();
                void SelectFirstTrack();

                std::shared_ptr<TrackListQueryBase> query;
                std::shared_ptr<musik::core::TrackList> tracks;
                HeaderCalculator headers;
                Adapter* adapter;
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
