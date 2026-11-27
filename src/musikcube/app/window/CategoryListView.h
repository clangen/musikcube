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

#include <musikcore/library.hpp>
#include <app/util/AsyncCallbackScope.h>

#include <sigslot/sigslot.h>

#include <cursespp/ListWindow.h>
#include <cursespp/ScrollAdapterBase.h>


#include <musikcore/playback.hpp>
#include <common/runtime/IMessage.h>

#include <mutex>

namespace musik {
    namespace cube {
        class CategoryListView:
            public cursespp::ListWindow,
            public sigslot::has_slots<>
        {
            public:
                using AddToPlaylist = std::function<void(const std::string&, const std::string&, int64_t)>;
                void SetAddToPlaylistCallback(AddToPlaylist callback) {
                    this->addToPlaylist = std::move(callback);
                }

                CategoryListView(
                    musik::core::api::Context& musikcore,
                    const std::string& fieldName);

                virtual ~CategoryListView();

                void RequeryWithField(
                    mc_track_search_mode matchType,
                    const std::string& fieldName,
                    const std::string& filter = "",
                    const int64_t selectAfterQuery = -1LL);

                void RequeryWithField(
                    const std::string& fieldName,
                    const std::string& filter = "",
                    const int64_t selectAfterQuery = -1LL);

                void Requery(
                    mc_track_search_mode matchType,
                    const std::string& filter = "",
                    const int64_t selectAfterQuery = -1LL);

                void Requery(
                    const std::string& filter,
                    const int64_t selectAfterQuery = -1LL);

                void Requery(const int64_t selectAfterQuery);

                void Requery();

                void Reset();
                void CancelQuery();

                int64_t GetSelectedId();
                std::string GetSelectedValue();
                std::string GetFilter();
                std::string GetFieldName();
                void SetFieldName(const std::string& fieldName);

                /* IWindow */
                bool KeyPress(const std::string& key) override;
                void OnVisibilityChanged(bool visible) override;
                void OnParentVisibilityChanged(bool visible) override;

            protected:
                /* IListWindow */
                cursespp::IScrollAdapter& GetScrollAdapter() override;
                bool OnEntryContextMenu(size_t index) override;

                void ApplyCategories(std::vector<musik::core::api::CategoryEntry> categories);
                void ReadCategoryPage(std::shared_ptr<std::vector<musik::core::api::CategoryEntry>> categories);
                void ShowContextMenu();
                class Adapter : public cursespp::ScrollAdapterBase {
                public:
                    Adapter(CategoryListView &parent);

                    size_t GetEntryCount() override;

                    cursespp::IScrollAdapter::EntryPtr
                        GetEntry(cursespp::ScrollableWindow* window, size_t index) override;

                private:
                    CategoryListView &parent;
                    cursespp::IScrollAdapter::ScrollPosition spos;
                };

            private:
                AddToPlaylist addToPlaylist;
                void RefreshPlayingCategory();
                bool IsPlayingCategory(const musik::core::api::CategoryEntry& entry) const;
                void ScrollToPlaying();

                Adapter *adapter;

                musik::core::api::Context& musikcore;
                musik::core::api::Playback playback;
                AsyncCallbackScope categoryWork;
                AsyncCallbackScope playingWork;
                mc_request_id activeQuery = 0;

                mc_request_id playingQuery = 0;
                std::optional<int64_t> playingCategoryId;
                std::string playingCategoryName;

                std::string fieldName, fieldIdColumn;
                std::string filter;
                mc_track_search_mode matchType{ MC_TRACK_SEARCH_SUBSTRING };
                int64_t selectAfterQuery;
                std::vector<musik::core::api::CategoryEntry> metadata;
        };
    }
}
