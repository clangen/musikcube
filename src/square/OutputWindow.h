#pragma once

#include "stdafx.h"
#include "BorderedWindow.h"

class OutputWindow : public BorderedWindow {
    public:
        OutputWindow();
        ~OutputWindow();

        void Write(const std::string& text);
        void WriteLine(const std::string& line);
};