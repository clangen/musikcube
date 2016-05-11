#pragma once

#include "stdafx.h"
#include "LogWindow.h"
#include "Colors.h"
#include "Screen.h"
#include <curses.h>

LogWindow::LogWindow() 
: ScrollableWindow() {
    this->SetContentColor(BOX_COLOR_WHITE_ON_BLUE);

    this->adapter = new SimpleScrollAdapter();
    this->adapter->SetDisplaySize(this->GetContentWidth(), this->GetContentHeight());
    this->adapter->SetMaxEntries(500);

    musik::debug::string_logged.connect(this, &LogWindow::OnLogged);
    musik::debug::warn("LogWindow", "initialized");
}

LogWindow::~LogWindow() {
}

IScrollAdapter& LogWindow::GetScrollAdapter() {
    return (IScrollAdapter&) *this->adapter;
}

void LogWindow::Update() {
    boost::mutex::scoped_lock lock(pendingMutex);

    if (!pending.size()) {
        return;
    }

    WINDOW* contents = this->GetContent();

    for (size_t i = 0; i < pending.size(); i++) {
        int64 attrs = COLOR_PAIR(BOX_COLOR_WHITE_ON_BLUE);

        LogEntry* entry = pending[i];

        switch (entry->level) {
            case musik::debug::level_error: {
                attrs = COLOR_PAIR(BOX_COLOR_RED_ON_BLUE) | A_BOLD;
                break;
            }

            case musik::debug::level_warning: {
                attrs = COLOR_PAIR(BOX_COLOR_YELLOW_ON_BLUE) | A_BOLD;
                break;
            }
        }

        std::string s = boost::str(boost::format(
            "[%1%] %2%") % entry->tag % entry->message);

        this->adapter->AddLine(s, attrs);

        delete entry;
    }

    this->OnAdapterChanged();
    pending.clear();
}

void LogWindow::OnLogged( /* from a background thread */
    musik::debug::log_level level,
    std::string tag,
    std::string message)
{
    boost::mutex::scoped_lock lock(pendingMutex);

    LogEntry *entry = new LogEntry();
    entry->level = level;
    entry->tag = tag;
    entry->message = message;
    pending.push_back(entry);
}