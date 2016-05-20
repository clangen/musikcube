#pragma once

#include "ILayout.h"
#include "ILayoutStack.h"
#include "Window.h"

#include <vector>

class LayoutBase : public Window, public ILayout {
    public:
        LayoutBase(IWindow* parent = NULL);
        virtual ~LayoutBase();

        /* IWindow */
        virtual void Show();
        virtual void Hide();
        virtual void Repaint();

        /* IOrderable */
        virtual void BringToTop();
        virtual void SendToBottom();

        /* ILayout */
        virtual IWindowPtr FocusNext();
        virtual IWindowPtr FocusPrev();
        virtual IWindowPtr GetFocus();
        virtual ILayoutStack* GetLayoutStack();
        virtual void SetLayoutStack(ILayoutStack* stack);

        virtual void Layout() = 0;

        /* IKeyHandler */
        virtual bool KeyPress(int64 ch);
        
        /* IWindowGroup */
        virtual bool AddWindow(IWindowPtr window);
        virtual bool RemoveWindow(IWindowPtr window);
        virtual size_t GetWindowCount();
        virtual IWindowPtr GetWindowAt(size_t position);

    private: 
        void AddFocusable(IWindowPtr window);
        void RemoveFocusable(IWindowPtr window);
        void SortFocusables();
        void IndexFocusables();

        ILayoutStack* layoutStack;
        std::vector<IWindowPtr> children;
        std::vector<IWindowPtr> focusable;
        int focused;
};