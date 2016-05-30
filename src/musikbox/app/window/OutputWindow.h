#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/ScrollableWindow.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/Colors.h>

namespace musik {
    namespace box {
        class OutputWindow :
            public cursespp::ScrollableWindow
#if ( __clang_major__==7 && __clang_minor__==3 )
            , public std::enable_shared_from_this<OutputWindow>
#endif
        {
            public:
                OutputWindow(cursespp::IWindow *parent = NULL);
                virtual ~OutputWindow();

                void WriteLine(const std::string& line, int64 attrs = -1);
                virtual cursespp::IScrollAdapter& GetScrollAdapter();

            private:
                cursespp::SimpleScrollAdapter* adapter;
        };
    }
}
