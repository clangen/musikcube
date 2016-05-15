#pragma once

#include "IScrollable.h"
#include "IScrollAdapter.h"
#include "ScrollableWindow.h"

class ListWindow : public ScrollableWindow {
    public:
        ListWindow(IWindow *parent = NULL);
        virtual ~ListWindow();

        virtual void ScrollToTop();
        virtual void ScrollToBottom();
        virtual void ScrollUp(int delta = 1);
        virtual void ScrollDown(int delta = 1);
        virtual void PageUp();
        virtual void PageDown();
        virtual void Focus();

        virtual size_t GetSelectedIndex();

    protected:
        virtual void OnAdapterChanged();
        virtual IScrollAdapter::ScrollPosition& GetScrollPosition();

    private:
        IScrollAdapter::ScrollPosition scrollPosition;
        size_t selectedIndex;
};