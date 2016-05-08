#pragma once 

#include "stdafx.h"

#include <boost/shared_ptr.hpp>
#include <curses.h>
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
        size_t lineCount, width, height;

        class Entry : public IEntry {
            public:
                Entry(const std::string& value);

                virtual size_t GetLineCount(size_t width);
                virtual std::string GetLine(size_t line, size_t width);
                virtual std::string GetValue();

            private:
                std::string value;
                size_t charCount;
        };

        typedef std::vector<boost::shared_ptr<Entry>> EntryList;
        typedef EntryList::iterator Iterator;

        EntryList entries;
};