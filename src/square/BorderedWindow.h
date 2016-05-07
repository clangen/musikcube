#pragma once

#include <curses.h>

class BorderedWindow {
    public:
        BorderedWindow();
        virtual ~BorderedWindow();

        void Create();
        void Destroy();
        virtual void Repaint();

    protected:
        WINDOW* GetContents() const;
        void SetSize(int width, int height);
        void SetPosition(int x, int y);
        void SetColor(int color);
        void SetScrollable(bool scrollable);
        void Clear();
        int GetWidth() const;
        int GetHeight() const;
        int GetX() const;
        int GetY() const;
        bool GetScrollable() const;

    private:
        WINDOW* border;
        WINDOW* contents;
        int width, height, x, y, color;
        bool scrollable;
};