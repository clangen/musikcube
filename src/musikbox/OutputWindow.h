#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/ScrollableWindow.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/Colors.h>

class OutputWindow : public ScrollableWindow {
    public:
        OutputWindow(IWindow *parent = NULL);
        ~OutputWindow();

        void WriteLine(const std::string& line, int64 attrs = -1);
        virtual IScrollAdapter& GetScrollAdapter();

    private:
        SimpleScrollAdapter* adapter;
};