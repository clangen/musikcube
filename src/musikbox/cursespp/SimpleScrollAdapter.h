#pragma once

#include "curses_config.h"
#include "ScrollAdapterBase.h"
#include <deque>

namespace cursespp {
    class SimpleScrollAdapter : public ScrollAdapterBase {
        public:
            SimpleScrollAdapter();
            virtual ~SimpleScrollAdapter();

            virtual void AddEntry(EntryPtr entry);
            virtual void SetMaxEntries(const size_t size = 500);
            virtual void Clear();

            virtual size_t GetEntryCount();
            virtual EntryPtr GetEntry(size_t index);

        private:
            typedef std::deque<EntryPtr> EntryList; /* TODO: this is O(n) lookup */
            typedef EntryList::iterator Iterator;

            EntryList entries;
            size_t maxEntries;
    };
}
