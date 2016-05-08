#pragma once 

#include "stdafx.h"

#include "IScrollAdapter.h";

class SimpleScrollAdapter : public IScrollAdapter {
    public:
        SimpleScrollAdapter();
        virtual ~SimpleScrollAdapter();

        virtual void SetDisplaySize(size_t width, size_t height);
        virtual size_t GetLineCount(size_t width);
        virtual size_t GetEntryCount();
        virtual void DrawPage(WINDOW* window, size_t index);
        virtual void AddLine(const std::string& str);

    private:
        class Entry {
            public:
                Entry(const std::string& value);

                size_t GetIndex();
                void SetIndex(size_t index);
                size_t GetLineCount(size_t width);
                std::string GetLine(size_t line, size_t width);
                std::string GetValue();

            private:
                size_t index;
                std::string value;
                size_t charCount;
        };


    private:
        typedef std::vector<boost::shared_ptr<Entry>> EntryList;
        typedef EntryList::iterator Iterator;


        void Reindex();
        size_t FindEntryIndex(int index);

        EntryList entries;
        size_t lineCount, width, height;
};