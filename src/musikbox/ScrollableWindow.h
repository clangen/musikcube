#pragma once

#include "curses_config.h"
#include "Window.h"
#include "IScrollAdapter.h"
#include "IScrollable.h"

class ScrollableWindow : public IScrollable, public Window {
    public:
        ScrollableWindow();
        ~ScrollableWindow();

        virtual void SetSize(int width, int height);

        virtual void ScrollToTop();
        virtual void ScrollToBottom();
        virtual void ScrollUp(int delta = 1);
        virtual void ScrollDown(int delta = 1);
        virtual void PageUp();
        virtual void PageDown();

        virtual void Create();

    protected:
        virtual IScrollAdapter& GetScrollAdapter() = 0;

        IScrollAdapter::ScrollPosition GetScrollPosition();

        void OnAdapterChanged();

    private:
        bool IsLastItemVisible();
        size_t GetPreviousPageEntryIndex();

        IScrollAdapter::ScrollPosition scrollPosition;
};