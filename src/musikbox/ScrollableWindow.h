#pragma once

#include "curses_config.h"
#include "Window.h"
#include "IScrollAdapter.h"

class ScrollableWindow : public Window {
    public:
        ScrollableWindow();
        ~ScrollableWindow();

        void ScrollToTop();
        void ScrollToBottom();
        void ScrollUp(int delta = 1);
        void ScrollDown(int delta = 1);
        void PageUp();
        void PageDown();

    protected:
        virtual IScrollAdapter& GetScrollAdapter() = 0;
        void OnAdapterChanged();

        size_t GetFirstVisible();
        size_t GetLastVisible();

    private:
        void CheckScrolledToBottom();

        size_t scrollPosition;
        bool scrolledToBottom;
};