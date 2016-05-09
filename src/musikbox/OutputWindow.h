#pragma once

#include "curses_config.h"
#include "ScrollableWindow.h"
#include "SimpleScrollAdapter.h"
#include "Colors.h"

class OutputWindow : public ScrollableWindow {
    public:
        OutputWindow();
        ~OutputWindow();

        void WriteLine(const std::string& line, int64 attrs = -1);
        virtual IScrollAdapter& GetScrollAdapter();

    private:
        SimpleScrollAdapter* adapter;
};