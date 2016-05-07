#pragma once

#include "stdafx.h"
#include "LogWindow.h"
#include "Colors.h"
#include "Screen.h"
#include <curses.h>

LogWindow::LogWindow() {
    this->SetColor(BOX_COLOR_WHITE_ON_BLUE);
    this->SetSize(Screen::GetWidth() / 2, Screen::GetHeight());
    this->SetPosition(Screen::GetWidth() / 2, 0);
    this->SetScrollable(true);

    this->Create();

    musik::debug::string_logged.connect(this, &LogWindow::OnLogged);
    musik::debug::err("LogWindow", "initialized");
}

LogWindow::~LogWindow() {
}

void LogWindow::Update() {
    boost::mutex::scoped_lock lock(pendingMutex);

    WINDOW* contents = this->GetContents();

    if (contents) {
        for (size_t i = 0; i < pending.size(); i++) {
            LogEntry* entry = pending[i];

            int64 color = COLOR_PAIR(BOX_COLOR_WHITE_ON_BLUE);

            switch (entry->level) {
            case musik::debug::level_error:
                color = COLOR_PAIR(BOX_COLOR_RED_ON_BLUE) | A_BOLD;
                break;

            case musik::debug::level_warning:
                color = COLOR_PAIR(BOX_COLOR_YELLOW_ON_BLUE) | A_BOLD;
                break;
            }

            wattron(contents, color);
            wprintw(contents, "[%s] %s\n", entry->tag.c_str(), entry->message.c_str());
            wattroff(contents, color);
            wrefresh(contents);

            delete entry;
        }
    }

    pending.clear();
}

void LogWindow::OnLogged(
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