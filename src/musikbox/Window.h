#pragma once

#include "curses_config.h"
#include "IWindow.h"

class Window : public IWindow, public std::enable_shared_from_this<IWindow> {
    public:
        Window(IWindow* parent = NULL);
        virtual ~Window();

        virtual void SetParent(IWindow* parent);

        virtual void Create();
        virtual void Destroy();

        virtual void Repaint();

        void SetFrameVisible(bool enabled);
        bool IsFrameVisible();

        virtual void SetContentColor(int color);
        virtual void SetFrameColor(int color);
        virtual void SetSize(int width, int height);
        virtual void SetPosition(int x, int y);

        virtual int GetWidth() const;
        virtual int GetHeight() const;
        virtual int GetContentHeight() const;
        virtual int GetContentWidth() const;
        virtual int GetX() const;
        virtual int GetY() const;

        virtual WINDOW* GetFrame() const;
        virtual WINDOW* GetContent() const;

    protected:
        IWindow* GetParent() const;

        void Clear();

    private:
        IWindow* parent;
        WINDOW* frame;
        WINDOW* content;
        bool drawFrame;
        int width, height, x, y, contentColor, frameColor;
};