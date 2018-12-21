//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include <stdafx.h>

#include "LogWindow.h"

#include <app/util/Messages.h>

#include <cursespp/MultiLineEntry.h>
#include <cursespp/Colors.h>
#include <cursespp/Screen.h>

using namespace musik::cube;
using namespace cursespp;

typedef IScrollAdapter::IEntry IEntry;

#define DEBOUNCE_REFRESH() this->Debounce(message::RefreshLogs, 0, 0, 250)

LogWindow::LogWindow(IWindow *parent)
: ScrollableWindow(nullptr, parent) {
    this->adapter = new SimpleScrollAdapter();
    this->adapter->SetMaxEntries(500);

    musik::debug::string_logged.connect(this, &LogWindow::OnLogged);
    musik::debug::warn("LogWindow", "initialized");
}

LogWindow::~LogWindow() {
    delete this->adapter;
}

IScrollAdapter& LogWindow::GetScrollAdapter() {
    return (IScrollAdapter&) *this->adapter;
}

void LogWindow::ClearContents() {
    this->adapter->Clear();
    this->OnAdapterChanged();
}

void LogWindow::ProcessMessage(musik::core::runtime::IMessage &message) {
    if (message.Type() == message::RefreshLogs) {
        this->Update();
    }
    else {
        ScrollableWindow::ProcessMessage(message);
    }
}

void LogWindow::OnVisibilityChanged(bool visible) {
    ScrollableWindow::OnVisibilityChanged(visible);

    if (visible) {
        DEBOUNCE_REFRESH();
    }
}

void LogWindow::Update() {
    std::unique_lock<std::mutex> lock(pendingMutex);

    if (!pending.size()) {
        return;
    }

    for (size_t i = 0; i < pending.size(); i++) {
        Color attrs = Color::TextDefault;

        LogEntry* entry = pending[i];

        switch (entry->level) {
            case musik::debug::level_error: {
                attrs = Color::TextError;
                break;
            }

            case musik::debug::level_warning: {
                attrs = Color::TextWarning;
                break;
            }
        }

        std::string s = boost::str(boost::format(
            "[%1%] %2%") % entry->tag % entry->message);

        this->adapter->AddEntry(std::shared_ptr<IEntry>(new MultiLineEntry(s, attrs)));

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
    std::unique_lock<std::mutex> lock(pendingMutex);

    LogEntry *entry = new LogEntry();
    entry->level = level;
    entry->tag = tag;
    entry->message = message;
    pending.push_back(entry);

    DEBOUNCE_REFRESH();
}
