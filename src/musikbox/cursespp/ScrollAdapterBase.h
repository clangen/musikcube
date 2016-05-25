#pragma once

#include "curses_config.h"
#include "IScrollAdapter.h"
#include <deque>

namespace cursespp {
    class ScrollAdapterBase : public IScrollAdapter {
        public:
            ScrollAdapterBase();
            virtual ~ScrollAdapterBase();

            virtual void SetDisplaySize(size_t width, size_t height);
            virtual size_t GetLineCount();
            virtual void DrawPage(WINDOW* window, size_t index, ScrollPosition *result = NULL);

            virtual size_t GetEntryCount() = 0;
            virtual EntryPtr GetEntry(size_t index) = 0;

        protected:
            void GetVisibleItems(size_t desired, std::deque<EntryPtr>& target, size_t& start);

            size_t GetWidth() { return this->width; }
            size_t GetHeight() { return this->height; }

        private:
            size_t width, height;
    };
}
