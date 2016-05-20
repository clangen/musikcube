#pragma once

#include "ILayout.h"

class WindowLayout : public ILayout, public std::enable_shared_from_this<ILayout> {
    public:
        WindowLayout(IWindowPtr window);
        virtual ~WindowLayout();

        /* ILayout */
        virtual IWindowPtr FocusNext();
        virtual IWindowPtr FocusPrev();
        virtual IWindowPtr GetFocus();
        virtual ILayoutStack* GetLayoutStack();
        virtual void SetLayoutStack(ILayoutStack* stack);
        virtual void Layout() { }

        /* IOrderable */
        virtual void BringToTop();
        virtual void SendToBottom();

        /* IDisplayable */
        virtual void Show();
        virtual void Hide();

        /* IKeyHandler */
        virtual bool KeyPress(int64 ch);

        /* IWindowGroup */
        virtual bool AddWindow(IWindowPtr window);
        virtual bool RemoveWindow(IWindowPtr window);
        virtual size_t GetWindowCount();
        virtual IWindowPtr GetWindowAt(size_t position);

    private:
        ILayoutStack* stack;
        IWindowPtr window;
};