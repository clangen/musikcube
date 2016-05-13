#pragma once

#include "curses_config.h"
#include "Window.h"
#include "IScrollAdapter.h"
#include "IScrollable.h"

class ScrollableWindow : public IScrollable, public Window {
    public:
        ScrollableWindow();
        virtual ~ScrollableWindow();

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
        virtual IScrollAdapter::ScrollPosition& GetScrollPosition();
        virtual void OnAdapterChanged();

        size_t GetPreviousPageEntryIndex();
        bool IsLastItemVisible();

    private:
        IScrollAdapter::ScrollPosition scrollPosition;
};