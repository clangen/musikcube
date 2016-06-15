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

#include <sigslot/sigslot.h>

#include <cursespp/ListWindow.h>
#include <cursespp/ScrollAdapterBase.h>

#include <app/query/CategoryListViewQuery.h>
#include <app/service/PlaybackService.h>

#include <core/library/IQuery.h>
#include <core/library/ILibrary.h>

using musik::core::IQueryPtr;
using musik::core::LibraryPtr;

using cursespp::IMessage;
using cursespp::ListWindow;
using cursespp::IScrollAdapter;
using cursespp::ScrollAdapterBase;

namespace musik {
    namespace box {
        class CategoryListView :
            public ListWindow,
#if (__clang_major__ == 7 && __clang_minor__ == 3)
            public std::enable_shared_from_this<CategoryListView>,
#endif
            public sigslot::has_slots<>
        {
            public:
                CategoryListView(
                    PlaybackService& playback,
                    LibraryPtr library, 
                    const std::string& fieldName);

                virtual ~CategoryListView();

                void RequeryWithField(
                    const std::string& fieldName,
                    const std::string& filter = "",
                    const DBID selectAfterQuery = 0);

                void Requery(
                    const std::string& filter = "",
                    const DBID selectAfterQuery = 0);

                void Reset();

                virtual void ProcessMessage(IMessage &message);

                DBID GetSelectedId();
                std::string GetFieldName();
                void SetFieldName(const std::string& fieldName);

            protected:
                virtual IScrollAdapter& GetScrollAdapter();
                void OnQueryCompleted(IQueryPtr query);

                class Adapter : public ScrollAdapterBase {
                public:
                    Adapter(CategoryListView &parent);

                    virtual size_t GetEntryCount();
                    virtual EntryPtr GetEntry(size_t index);

                private:
                    CategoryListView &parent;
                    IScrollAdapter::ScrollPosition spos;
                };

            private:
                void OnTrackChanged(size_t index, musik::core::TrackPtr track);

                PlaybackService& playback;
                LibraryPtr library;
                Adapter *adapter;

                boost::mutex queryMutex;
                DBID selectAfterQuery;
                std::shared_ptr<CategoryListViewQuery> activeQuery;

                musik::core::TrackPtr playing;

                std::string fieldName;
                CategoryListViewQuery::ResultList metadata;
        };
    }
}
