//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/IKeyHandler.h>
#include <cursespp/ListWindow.h>

#include <app/query/TrackListQueryBase.h>
#include <app/service/PlaybackService.h>

#include <core/library/ILibrary.h>

namespace musik {
    namespace box {
        class TrackListView :
            public cursespp::ListWindow,
#if (__clang_major__ == 7 && __clang_minor__ == 3)
            public std::enable_shared_from_this<TrackListView>,
#endif
            public sigslot::has_slots<>
        {
            public:
                sigslot::signal0<> Requeried;

                typedef std::function<std::string(
                    musik::core::TrackPtr, size_t)> RowFormatter;

                typedef std::shared_ptr<std::set<size_t> > Headers;

                TrackListView(
                    PlaybackService& playback,
                    musik::core::LibraryPtr library,
                    RowFormatter formatter = RowFormatter());

                virtual ~TrackListView();

                virtual void ProcessMessage(cursespp::IMessage &message);
                std::shared_ptr<TrackList> GetTrackList();
                void Clear();

                void Requery(std::shared_ptr<TrackListQueryBase> query);

            protected:
                virtual cursespp::IScrollAdapter& GetScrollAdapter();
                void OnQueryCompleted(musik::core::IQueryPtr query);

                class Adapter : public cursespp::ScrollAdapterBase {
                    public:
                        Adapter(TrackListView &parent);

                        virtual size_t GetEntryCount();
                        virtual EntryPtr GetEntry(size_t index);

                    private:
                        TrackListView &parent;
                        IScrollAdapter::ScrollPosition spos;
                };

            private:
                void OnTrackChanged(size_t index, musik::core::TrackPtr track);

                std::shared_ptr<TrackListQueryBase> query;
                std::shared_ptr<TrackList> metadata;
                Headers headers;
                Adapter* adapter;
                PlaybackService& playback;
                musik::core::TrackPtr playing;
                musik::core::LibraryPtr library;
                size_t lastQueryHash;
                RowFormatter formatter;
        };
    }
}
