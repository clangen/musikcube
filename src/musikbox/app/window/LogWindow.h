#pragma once

#include <sigslot/sigslot.h>

#include <core/debug.h>

#include <boost/thread/mutex.hpp>

#include <cursespp/ScrollableWindow.h>
#include <cursespp/SimpleScrollAdapter.h>

namespace musik {
    namespace box {
        class LogWindow : public cursespp::ScrollableWindow, public sigslot::has_slots<> {
            public:
                LogWindow(cursespp::IWindow *parent = NULL);
                ~LogWindow();

                void Update();

            protected:
                virtual cursespp::IScrollAdapter& GetScrollAdapter();

            private:
                void OnLogged(
                    musik::debug::log_level level,
                    std::string tag,
                    std::string message);

                struct LogEntry {
                    int level;
                    std::string tag;
                    std::string message;
                };

                boost::mutex pendingMutex;
                std::vector<LogEntry*> pending;
                cursespp::SimpleScrollAdapter* adapter;
        };
    }
}
