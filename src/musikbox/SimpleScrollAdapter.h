#pragma once 

#include "curses_config.h"
#include "IScrollAdapter.h";
#include <deque>

class SimpleScrollAdapter : public IScrollAdapter {
    public:
        SimpleScrollAdapter();
        virtual ~SimpleScrollAdapter();

        virtual void SetDisplaySize(size_t width, size_t height);
        virtual size_t GetLineCount();
        virtual size_t GetEntryCount();
        virtual void DrawPage(WINDOW* window, size_t index);

        virtual void AddLine(const std::string& str, int64 attrs = -1);
        virtual void SetMaxEntries(const size_t size = 500);

    private:
        class Entry {
            public:
                Entry(const std::string& value);

                size_t GetIndex();
                void SetIndex(size_t index);
                size_t GetLineCount();
                std::string GetLine(size_t line);
                std::string GetValue();
                void SetWidth(size_t width);
                void SetAttrs(int64 attrs);
                int64 GetAttrs();

            private:
                size_t index;
                std::string value;
                std::vector<std::string> lines;
                size_t charCount;
                int64 attrs;
                size_t width;
        };

    private:
        typedef std::deque<boost::shared_ptr<Entry>> EntryList;
        typedef EntryList::iterator Iterator;


        void Reindex();
        size_t FindEntryIndex(size_t index);

        EntryList entries;
        size_t lineCount, width, height;
        size_t removedOffset;
        size_t maxEntries;
};
