#pragma once

#include "curses_config.h"

class BorderedWindow {
    public:
        BorderedWindow();
        virtual ~BorderedWindow();

        void Create();
        void Destroy();
        virtual void Repaint();

        virtual void SetContentColor(int color);
        virtual void SetBorderColor(int color);

    protected:
        WINDOW* GetContents() const;
        void SetSize(int width, int height);
        void SetPosition(int x, int y);

        void SetScrollable(bool scrollable);
        void Clear();
        int GetWidth() const;
        int GetHeight() const;
        int GetContentHeight() const;
        int GetContentWidth() const;
        int GetX() const;
        int GetY() const;
        bool GetScrollable() const;

    private:
        WINDOW* border;
        WINDOW* contents;
        int width, height, x, y, contentColor, borderColor;
        bool scrollable;
};