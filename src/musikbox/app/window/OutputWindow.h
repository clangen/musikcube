#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/ScrollableWindow.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/Colors.h>

namespace musik {
    namespace box {
        class OutputWindow : public cursespp::ScrollableWindow {
        public:
            OutputWindow(cursespp::IWindow *parent = NULL);
            ~OutputWindow();

            void WriteLine(const std::string& line, int64 attrs = -1);
            virtual cursespp::IScrollAdapter& GetScrollAdapter();

        private:
            cursespp::SimpleScrollAdapter* adapter;
        };
    }
}
