#pragma once

#include "curses_config.h"
#include "IDisplayable.h"

class IWindow : public IDisplayable {
    public:
        virtual void Repaint() = 0;

        virtual void SetParent(IWindow* parent) = 0;

        virtual void Show() = 0;
        virtual void Hide() = 0;

        virtual void Focus() = 0;
        virtual void Blur() = 0;

        virtual void SetContentColor(int64 color) = 0;
        virtual void SetFrameColor(int64 color) = 0;

        virtual void SetSize(int width, int height) = 0;
        virtual void SetPosition(int x, int y) = 0;

        virtual int GetWidth() const = 0;
        virtual int GetHeight() const = 0;

        virtual int GetContentHeight() const = 0;
        virtual int GetContentWidth() const = 0;

        virtual int GetX() const = 0;
        virtual int GetY() const = 0;

        virtual int GetId() const = 0;

        virtual int GetFocusOrder() = 0;
        virtual void SetFocusOrder(int order = -1) = 0;

        virtual WINDOW* GetFrame() const = 0;
        virtual WINDOW* GetContent() const = 0;
};

typedef std::shared_ptr<IWindow> IWindowPtr;