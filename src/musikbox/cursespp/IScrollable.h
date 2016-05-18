#pragma once

class IScrollable {
    public:
        virtual ~IScrollable() = 0 { }
        virtual void ScrollToTop() = 0;
        virtual void ScrollToBottom() = 0;
        virtual void ScrollUp(int delta = 1) = 0;
        virtual void ScrollDown(int delta = 1) = 0;
        virtual void PageUp() = 0;
        virtual void PageDown() = 0;
};