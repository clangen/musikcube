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

/**
 * @file CategoryListView.h
 * @brief List window that displays a library category's values.
 * @details Queries the library for the values of a category (album, artist,
 *          genre, etc.), supports substring filtering and shows a context
 *          menu on the selected entry. Keeps the playing track highlighted
 *          and scrolls to it when playback changes.
 */

#include <sigslot/sigslot.h>

#include <cursespp/ListWindow.h>
#include <cursespp/ScrollAdapterBase.h>

#include <musikcore/library/query/CategoryListQuery.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/runtime/IMessage.h>

#include <mutex>

namespace musik {
    namespace cube {
        /**
         * @brief A scrollable list of category values.
         * @details Renders the values of a single library category. The list
         *          can be requeried with a different field, filter or match
         *          type, and after each query the selection can be restored.
         *          The currently playing track is highlighted when it appears
         *          in the list.
         */
        class CategoryListView:
            public cursespp::ListWindow,
            public sigslot::has_slots<>
        {
            public:
                using MatchType = musik::core::library::query::QueryBase::MatchType;

                /**
                 * @brief Creates the list bound to the given playback service
                 *        and library.
                 * @param playback the playback service providing track changes
                 * @param library the library that provides the category data
                 * @param fieldName the name of the category field to display
                 */
                CategoryListView(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    const std::string& fieldName);

                /**
                 * @brief Destroys the list and its adapter.
                 */
                virtual ~CategoryListView();

                /**
                 * @brief Re-queries the list with the given field and filter.
                 * @param matchType the query match type
                 * @param fieldName the category field to display
                 * @param filter an optional substring filter
                 * @param selectAfterQuery the id to select when done, or -1
                 */
                void RequeryWithField(
                    MatchType matchType,
                    const std::string& fieldName,
                    const std::string& filter = "",
                    const int64_t selectAfterQuery = -1LL);

                /**
                 * @brief Re-queries the list with the given field, keeping the
                 *        current match type.
                 * @param fieldName the category field to display
                 * @param filter an optional substring filter
                 * @param selectAfterQuery the id to select when done, or -1
                 */
                void RequeryWithField(
                    const std::string& fieldName,
                    const std::string& filter = "",
                    const int64_t selectAfterQuery = -1LL);

                /**
                 * @brief Re-queries the list with the given match type and
                 *        filter, keeping the current field.
                 * @param matchType the query match type
                 * @param filter an optional substring filter
                 * @param selectAfterQuery the id to select when done, or -1
                 */
                void Requery(
                    MatchType matchType,
                    const std::string& filter = "",
                    const int64_t selectAfterQuery = -1LL);

                /**
                 * @brief Re-queries the list with the given filter, keeping
                 *        the current field and match type.
                 * @param filter an optional substring filter
                 * @param selectAfterQuery the id to select when done, or -1
                 */
                void Requery(
                    const std::string& filter,
                    const int64_t selectAfterQuery = -1LL);

                /**
                 * @brief Re-queries the list and selects an entry afterwards.
                 * @param selectAfterQuery the id to select when done, or -1
                 */
                void Requery(const int64_t selectAfterQuery);

                /**
                 * @brief Re-queries the list with the current settings.
                 */
                void Requery();

                /**
                 * @brief Clears the list and cancels the active query.
                 */
                void Reset();

                /**
                 * @brief Returns the id of the selected entry.
                 * @return the selected entry id, or -1
                 */
                int64_t GetSelectedId();
                /**
                 * @brief Returns the value of the selected entry.
                 * @return the selected entry value
                 */
                std::string GetSelectedValue();
                /**
                 * @brief Returns the active filter text.
                 * @return the filter string
                 */
                std::string GetFilter();
                /**
                 * @brief Returns the category field being displayed.
                 * @return the field name
                 */
                std::string GetFieldName();
                /**
                 * @brief Changes the category field being displayed.
                 * @param fieldName the new field name
                 */
                void SetFieldName(const std::string& fieldName);

                /* IWindow */
                /**
                 * @brief Handles keyboard input.
                 * @param key the key sequence that was pressed
                 * @return true if the event was consumed
                 */
                bool KeyPress(const std::string& key) override;
                /**
                 * @brief Called when the list becomes visible or hidden.
                 * @param visible true if the list became visible
                 */
                void OnVisibilityChanged(bool visible) override;

            protected:
                /* IListWindow */
                /**
                 * @brief Returns the scroll adapter feeding the list.
                 * @return the adapter
                 */
                cursespp::IScrollAdapter& GetScrollAdapter() override;
                /**
                 * @brief Handles the context menu on the selected entry.
                 * @param index the index of the entry
                 * @return true if a menu was shown
                 */
                bool OnEntryContextMenu(size_t index) override;

                /**
                 * @brief Handles completion of the category query.
                 * @param query the completed query
                 */
                void OnQueryCompleted(musik::core::db::IQuery* query);
                /**
                 * @brief Shows the context menu for the selection.
                 */
                void ShowContextMenu();

                /**
                 * @brief Adapter that renders the category entries.
                 */
                class Adapter : public cursespp::ScrollAdapterBase {
                public:
                    /**
                     * @brief Creates the adapter for the given parent list.
                     * @param parent the owning CategoryListView
                     */
                    Adapter(CategoryListView &parent);

                    /**
                     * @brief Returns the number of entries.
                     * @return the entry count
                     */
                    size_t GetEntryCount() override;

                    /**
                     * @brief Returns the entry for the given index.
                     * @param window the owning scrollable window
                     * @param index the entry index
                     * @return the entry to render
                     */
                    cursespp::IScrollAdapter::EntryPtr
                        GetEntry(cursespp::ScrollableWindow* window, size_t index) override;

                private:
                    CategoryListView &parent;                       /**< the owning list view */
                    cursespp::IScrollAdapter::ScrollPosition spos;  /**< the persisted scroll position */
                };

            private:
                /**
                 * @brief Highlights the entry when the playing track changes.
                 * @param index the index of the changed track
                 * @param track the changed track
                 */
                void OnTrackChanged(size_t index, musik::core::TrackPtr track);
                /**
                 * @brief Scrolls the list so the playing entry is visible.
                 */
                void ScrollToPlaying();

                musik::core::audio::PlaybackService& playback; /**< the playback service */
                Adapter *adapter;                               /**< the scroll adapter */

                std::shared_ptr<musik::core::library::query::CategoryListQuery> activeQuery; /**< the active query */

                musik::core::ILibraryPtr library;               /**< the library providing the data */
                musik::core::TrackPtr playing;                  /**< the currently playing track */

                std::string fieldName, fieldIdColumn;           /**< the category field and its id column */
                std::string filter;                             /**< the active filter text */
                MatchType matchType{ MatchType::Substring };    /**< the current match type */
                int64_t selectAfterQuery;                       /**< the id to select after a query */
                musik::core::library::query::CategoryListQuery::Result metadata; /**< the last query result */
        };
    }
}
