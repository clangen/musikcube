#pragma once

#include "curses_config.h"
#include "IWindow.h"

class Window : public IWindow {
    public:
        Window();
        virtual ~Window();

        void Create();
        void Destroy();

        virtual void Repaint();

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
        void Clear();

    private:
        WINDOW* frame;
        WINDOW* content;
        int width, height, x, y, contentColor, frameColor;
};