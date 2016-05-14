#pragma once

#include "ILayout.h"
#include "Window.h"

#include <vector>

class LayoutBase : public Window, public ILayout {
    public:
        LayoutBase(IWindow* parent = NULL);
        virtual ~LayoutBase();

        /* IWindow */
        virtual void Show();
        virtual void Hide();

        /* ILayout */
        virtual IWindow* FocusNext();
        virtual IWindow* FocusPrev();
        virtual IWindow* GetFocus();

        virtual void Layout() = 0;
        virtual void OnIdle() = 0;

        /* IWindowGroup */
        virtual bool AddWindow(IWindowPtr window);
        virtual bool RemoveWindow(IWindowPtr window);
        virtual size_t GetWindowCount();
        virtual IWindowPtr GetWindowAt(size_t position);

    private: 
        void AddFocusable(IWindowPtr window);
        void RemoveFocusable(IWindowPtr window);

        std::vector<IWindowPtr> children;
        std::vector<IWindowPtr> focusable;
        int focused;
};