#pragma once

#include "curses_config.h"
#include "IWindow.h"

class Window : public IWindow, public std::enable_shared_from_this<IWindow> {
    public:
        Window(IWindow* parent = NULL);
        virtual ~Window();

        virtual void SetParent(IWindow* parent);

        virtual void Show();
        virtual void Hide();

        virtual void Repaint();

        void SetFrameVisible(bool enabled);
        bool IsFrameVisible();

        virtual void Focus();
        virtual void Blur();

        virtual void SetContentColor(int64 color);
        virtual void SetFrameColor(int64 color);
        virtual void SetSize(int width, int height);
        virtual void SetPosition(int x, int y);

        virtual int GetWidth() const;
        virtual int GetHeight() const;
        virtual int GetContentHeight() const;
        virtual int GetContentWidth() const;
        virtual int GetX() const;
        virtual int GetY() const;
        virtual int GetId() const;

        virtual WINDOW* GetFrame() const;
        virtual WINDOW* GetContent() const;

        virtual int GetFocusOrder();
        virtual void SetFocusOrder(int order = -1);

        static void WriteToScreen();

    protected:
        IWindow* GetParent() const;

        void Clear();

    private:
        IWindow* parent;
        WINDOW* frame;
        WINDOW* content;
        bool drawFrame;
        int focusOrder;
        int id;
        int64 contentColor, frameColor;
        int width, height, x, y;
};