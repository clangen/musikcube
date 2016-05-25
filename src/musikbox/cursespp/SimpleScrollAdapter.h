#pragma once 

#include "curses_config.h"
#include "ScrollAdapterBase.h"
#include <deque>

class SimpleScrollAdapter : public ScrollAdapterBase {
    public:
        SimpleScrollAdapter();
        virtual ~SimpleScrollAdapter();

        virtual void AddEntry(EntryPtr entry);
        virtual void SetMaxEntries(const size_t size = 500);

        virtual size_t GetEntryCount();
        virtual EntryPtr GetEntry(size_t index);

    private:
        typedef std::deque<EntryPtr> EntryList;
        typedef EntryList::iterator Iterator;

        EntryList entries;
        size_t maxEntries;
};
