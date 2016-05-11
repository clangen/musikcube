#pragma once

#include "curses_config.h"
#include "Window.h"
#include "IScrollAdapter.h"
#include "IScrollable.h"

class ScrollableWindow : public IScrollable, public Window {
    public:
        ScrollableWindow();
        ~ScrollableWindow();

        virtual void ScrollToTop();
        virtual void ScrollToBottom();
        virtual void ScrollUp(int delta = 1);
        virtual void ScrollDown(int delta = 1);
        virtual void PageUp();
        virtual void PageDown();

        virtual void Create();

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