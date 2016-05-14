#pragma once

#include "ILayout.h"
#include "Window.h"

#include <vector>

class LayoutBase : public Window, public ILayout {
    public:
        LayoutBase(IWindow* parent = NULL);
        virtual ~LayoutBase();

        /* IWindow */
        virtual void Create();
        virtual void Destroy();

        /* ILayout */
        virtual IWindow* FocusNext() = 0;
        virtual IWindow* FocusPrev() = 0;
        virtual IWindow* GetFocus() = 0;
        virtual void Layout() = 0;

        virtual void Hide();
        virtual void Show();

        virtual void OnIdle() = 0;

        /* IWindowGroup */
        virtual bool AddWindow(IWindowPtr window);
        virtual bool RemoveWindow(IWindowPtr window);
        virtual size_t GetWindowCount();
        virtual IWindowPtr GetWindowAt(size_t position);

    private: 
        std::vector<IWindowPtr> children;
};