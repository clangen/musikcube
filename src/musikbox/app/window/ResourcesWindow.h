#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/Window.h>

#include <app/util/SystemInfo.h>

namespace musik {
    namespace box {
        class ResourcesWindow :
            public cursespp::Window
#if (__clang_major__ == 7 && __clang_minor__ == 3)
            , public std::enable_shared_from_this<ResourcesWindow>
#endif
        {
            public:
                ResourcesWindow(cursespp::IWindow *parent = NULL);
                virtual ~ResourcesWindow();

                virtual void Update();

            private:
                SystemInfo* systemInfo;
        };
    }
}
