#pragma once

#include "stdafx.h"
#include "ScrollableWindow.h"
#include "SimpleScrollAdapter.h"

class OutputWindow : public ScrollableWindow {
    public:
        OutputWindow();
        ~OutputWindow();

        void WriteLine(const std::string& line);
        virtual IScrollAdapter& GetScrollAdapter();

    private:
        SimpleScrollAdapter* adapter;
};