#pragma once 

#include "curses_config.h"
#include "IScrollAdapter.h"
#include <deque>

class SimpleScrollAdapter : public IScrollAdapter {
    public:
        SimpleScrollAdapter();
        virtual ~SimpleScrollAdapter();

        virtual void SetDisplaySize(size_t width, size_t height);
        virtual size_t GetLineCount();
        virtual size_t GetEntryCount();
        virtual void DrawPage(WINDOW* window, size_t index);

        virtual void AddEntry(boost::shared_ptr<IEntry> entry);
        virtual void SetMaxEntries(const size_t size = 500);

    private:
        typedef std::deque<boost::shared_ptr<IEntry>> EntryList;
        typedef EntryList::iterator Iterator;

        void Reindex();
        size_t FindEntryIndex(size_t index);

        EntryList entries;
        size_t lineCount, width, height;
        size_t removedOffset;
        size_t maxEntries;
};
