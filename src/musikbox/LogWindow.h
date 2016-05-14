#pragma once

#include <sigslot/sigslot.h>

#include <core/debug.h>
#include <boost/thread/mutex.hpp>
#include "ScrollableWindow.h"
#include "SimpleScrollAdapter.h"

class LogWindow : public ScrollableWindow, public sigslot::has_slots<> {
    public:
        LogWindow(IWindow *parent = NULL);
        ~LogWindow();

        void Update();

    protected:
        virtual IScrollAdapter& GetScrollAdapter();

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
        SimpleScrollAdapter* adapter;
};