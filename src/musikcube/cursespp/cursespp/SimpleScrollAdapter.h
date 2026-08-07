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

/** @file SimpleScrollAdapter.h @brief A simple string-backed scroll adapter for lists. */
#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/Colors.h>
#include <cursespp/ScrollAdapterBase.h>
#include <sigslot/sigslot.h>
#include <deque>
#include <map>

namespace cursespp {
    /** @brief A ScrollAdapterBase whose entries are single-line strings.
     *
     *  @details SimpleScrollAdapter stores a deque of string-backed IEntry
     *  objects and exposes them to a ScrollableWindow/ListWindow. It can cap
     *  the number of kept entries (SetMaxEntries, default 500), track per-index
     *  colors, and optionally participate in selection highlighting. When its
     *  contents change it emits the Changed signal so owning windows can
     *  invalidate and redraw. Lookup is O(n) by design (see the note in the
     *  code); it is intended for small-to-medium lists such as search results
     *  or command history.
     */
    class SimpleScrollAdapter : public ScrollAdapterBase {
        public:
            /** @brief Fired whenever the adapter's contents change. */
            sigslot::signal1<SimpleScrollAdapter*> Changed;

            /** @brief Creates an empty adapter. */
            SimpleScrollAdapter();
            /** @brief Destroys the adapter. */
            virtual ~SimpleScrollAdapter();

            /** @brief Returns the number of stored entries.
             *  @return the entry count.
             */
            size_t GetEntryCount() override;
            /** @brief Returns the entry at a given index.
             *  @param window the requesting ScrollableWindow (may be unused).
             *  @param index the entry index.
             *  @return the EntryPtr at that index.
             */
            EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) override;

            /** @brief Controls whether the entries can be highlighted as selected.
             *  @param selectable true to mark entries as selectable.
             */
            void SetSelectable(bool selectable);
            /** @brief Appends a string entry.
             *  @param entry the text of the new entry.
             */
            void AddEntry(const std::string& entry);
            /** @brief Returns the string stored at a given index.
             *  @param index the entry index.
             *  @return the entry's text.
             */
            std::string StringAt(size_t index);

            /* virtual methods we define */
            /** @brief Appends a pre-built entry.
             *  @param entry the EntryPtr to append.
             */
            virtual void AddEntry(EntryPtr entry);
            /** @brief Caps the number of entries kept by the adapter.
             *  @param size the maximum number of entries (default 500).
             */
            virtual void SetMaxEntries(const size_t size = 500);
            /** @brief Removes all entries. */
            virtual void Clear();

        private:
            typedef std::deque<EntryPtr> EntryList; /* TODO: this is O(n) lookup */
            typedef EntryList::iterator Iterator;
            std::map<size_t, Color> indexToColor;  /**< Optional per-index color overrides. */
            EntryList entries;                     /**< The stored entries. */
            size_t maxEntries;                     /**< The maximum number of kept entries. */
            bool selectable;                       /**< Whether entries participate in selection. */
    };
}
