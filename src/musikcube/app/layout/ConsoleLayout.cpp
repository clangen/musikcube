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

#include "stdafx.h"

#include "ConsoleLayout.h"

#include <cursespp/App.h>
#include <cursespp/Screen.h>
#include <cursespp/MultiLineEntry.h>
#include <cursespp/Colors.h>

#include <core/sdk/IPlugin.h>
#include <core/plugin/PluginFactory.h>

#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <app/version.h>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

template <class T>
bool tostr(T& t, const std::string& s) {
    std::istringstream iss(s);
    return !(iss >> t).fail();
}

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace cursespp;

typedef IScrollAdapter::EntryPtr EntryPtr;

ConsoleLayout::ConsoleLayout(ITransport& transport, ILibraryPtr library)
: LayoutBase()
, transport(transport)
, library(library) {
    this->SetFrameVisible(false);

    this->outputAdapter.reset(new SimpleScrollAdapter());
    this->outputAdapter->SetMaxEntries(500);

    this->logs.reset(new LogWindow(this));
    this->logs->SetFrameTitle(_TSTR("console_debug_logs_title"));
    this->output.reset(new ScrollableWindow(this->outputAdapter, this));
    this->output->SetFrameTitle(_TSTR("console_command_output_title"));
    this->commands.reset(new cursespp::TextInput());
    this->commands->SetFrameTitle(_TSTR("console_command_title"));

    this->commands->SetFocusOrder(0);
    this->output->SetFocusOrder(1);
    this->logs->SetFocusOrder(2);

    this->AddWindow(this->commands);
    this->AddWindow(this->logs);
    this->AddWindow(this->output);

    this->commands->EnterPressed.connect(this, &ConsoleLayout::OnEnterPressed);

    this->Help();
}

ConsoleLayout::~ConsoleLayout() {

}

void ConsoleLayout::OnLayout() {
    const int cx = this->GetWidth();
    const int cy = this->GetHeight();

    const int leftCx = cx / 2;
    const int rightCx = cx - leftCx;

    /* top left */
    this->output->MoveAndResize(0, 0, leftCx, cy - 3);

    /* bottom left */
    this->commands->MoveAndResize(0, cy - 3, leftCx, 3);

    /* right */
    this->logs->MoveAndResize(cx / 2, 0, rightCx, cy);
}

void ConsoleLayout::SetShortcutsWindow(ShortcutsWindow* shortcuts) {
    if (shortcuts) {
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateConsole), _TSTR("shortcuts_console"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibrary), _TSTR("shortcuts_library"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateSettings), _TSTR("shortcuts_settings"));
        shortcuts->AddShortcut("^D", _TSTR("shortcuts_quit"));

        shortcuts->SetChangedCallback([this](std::string key) {
            if (Hotkeys::Is(Hotkeys::NavigateSettings, key)) {
                this->BroadcastMessage(message::JumpToSettings);
            }
            if (Hotkeys::Is(Hotkeys::NavigateLibrary, key)) {
                this->BroadcastMessage(message::JumpToLibrary);
            }
            else if (key == "^D") {
                App::Instance().Quit();
            }
            else {
                this->KeyPress(key);
            }
        });

        shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateConsole));
    }
}

void ConsoleLayout::WriteOutput(const std::string& str, int64_t attrs) {
    this->outputAdapter->AddEntry(EntryPtr(new MultiLineEntry(str, attrs)));
    this->output->OnAdapterChanged();
}

void ConsoleLayout::OnEnterPressed(TextInput *input) {
    std::string command = this->commands->GetText();
    this->commands->SetText("");

    this->WriteOutput("> " + command + "\n", COLOR_PAIR(CURSESPP_TEXT_DEFAULT));

    if (!this->ProcessCommand(command)) {
        if (command.size()) {
            this->WriteOutput(
                "illegal command: '" +
                command +
                "'\n", COLOR_PAIR(CURSESPP_TEXT_ERROR));
        }
    }
}


void ConsoleLayout::Seek(const std::vector<std::string>& args) {
    if (args.size() > 0) {
        double newPosition = 0;
        if (tostr<double>(newPosition, args[0])) {
            transport.SetPosition(newPosition);
        }
    }
}

void ConsoleLayout::SetVolume(const std::vector<std::string>& args) {
    if (args.size() > 0) {
        float newVolume = 0;
        if (tostr<float>(newVolume, args[0])) {
            this->SetVolume(newVolume / 100.0f);
        }
    }
}

void ConsoleLayout::SetVolume(float volume) {
    transport.SetVolume(volume);
}

void ConsoleLayout::Help() {
    int64_t s = -1;

    this->WriteOutput("help:\n", s);
    this->WriteOutput("  <tab> to switch between windows", s);
    this->WriteOutput("", s);
    this->WriteOutput("  addir <dir>: add a music directory", s);
    this->WriteOutput("  rmdir <dir>: remove a music directory", s);
    this->WriteOutput("  lsdirs: list scanned directories", s);
    this->WriteOutput("  rescan: rescan paths for new metadata", s);
    this->WriteOutput("", s);
    this->WriteOutput("  play <uri>: play audio at <uri>", s);
    this->WriteOutput("  pause: pause/resume", s);
    this->WriteOutput("  stop: stop and clean up everything", s);
    this->WriteOutput("  volume: <0 - 100>: set % volume", s);
    this->WriteOutput("  clear: clear the log window", s);
    this->WriteOutput("  seek <seconds>: seek to <seconds> into track", s);
    this->WriteOutput("", s);
    this->WriteOutput("  plugins: list loaded plugins", s);
    this->WriteOutput("", s);
    this->WriteOutput("  version: show musikcube app version", s);
    this->WriteOutput("", s);
    this->WriteOutput("  <ctrl+d>: quit\n", s);
}

bool ConsoleLayout::ProcessCommand(const std::string& cmd) {
    std::vector<std::string> args;
    boost::algorithm::split(args, cmd, boost::is_any_of(" "));

    auto it = args.begin();
    while (it != args.end()) {
        std::string trimmed = boost::algorithm::trim_copy(*it);
        if (trimmed.size()) {
            *it = trimmed;
            ++it;
        }
        else {
            it = args.erase(it);
        }
    }

    if (args.size() == 0) {
        return true;
    }

    std::string name = args.size() > 0 ? args[0] : "";
    args.erase(args.begin());

    if (name == "plugins") {
        this->ListPlugins();
    }
    else if (name == "clear") {
        this->logs->ClearContents();
    }
    else if (name == "version") {
        const std::string v = boost::str(boost::format("v%s") % VERSION);
        this->WriteOutput(v, -1);
    }
    else if (name == "play" || name == "pl" || name == "p") {
        return this->PlayFile(args);
    }
    else if (name == "addir") {
        std::string path = boost::algorithm::join(args, " ");
        library->Indexer()->AddPath(path);
    }
    else if (name == "rmdir") {
        std::string path = boost::algorithm::join(args, " ");
        library->Indexer()->RemovePath(path);
    }
    else if (name == "lsdirs") {
        std::vector<std::string> paths;
        library->Indexer()->GetPaths(paths);
        this->WriteOutput("paths:");
        for (size_t i = 0; i < paths.size(); i++) {
            this->WriteOutput("  " + paths.at(i));
        }
        this->WriteOutput("");
    }
    else if (name == "rescan" || name == "scan" || name == "index") {
        library->Indexer()->Schedule(IIndexer::SyncType::All);
    }
    else if (name == "h" || name == "help") {
        this->Help();
    }
    else if (name == "pa" || name == "pause") {
        this->Pause();
    }
    else if (name == "s" || name =="stop") {
        this->Stop();
    }
    else if (name == "sk" || name == "seek") {
        this->Seek(args);
    }
    else if (name == "plugins") {
        this->ListPlugins();
    }
    else if (name == "sdk") {
        this->WriteOutput("    sdk/api revision: " +
            std::to_string(musik::core::sdk::SdkVersion));
    }
    else if (name == "v" || name == "volume") {
        this->SetVolume(args);
    }
    else {
        return false;
    }

    return true;
}

bool ConsoleLayout::PlayFile(const std::vector<std::string>& args) {
    if (args.size() > 0) {
        std::string filename = boost::algorithm::join(args, " ");
        transport.Start(filename, ITransport::Gain(), ITransport::StartMode::Immediate);
        return true;
    }

    return false;
}

void ConsoleLayout::Pause() {
    int state = this->transport.GetPlaybackState();
    if (state == PlaybackPaused) {
        this->transport.Resume();
    }
    else if (state == PlaybackPlaying) {
        this->transport.Pause();
    }
}

void ConsoleLayout::Stop() {
    this->transport.Stop();
}

void ConsoleLayout::ListPlugins() {
    using musik::core::sdk::IPlugin;
    using musik::core::PluginFactory;

    typedef std::vector<std::shared_ptr<IPlugin> > PluginList;
    typedef PluginFactory::NullDeleter<IPlugin> Deleter;

    PluginList plugins = PluginFactory::Instance()
        .QueryInterface<IPlugin, Deleter>("GetPlugin");

    PluginList::iterator it = plugins.begin();
    for (; it != plugins.end(); it++) {
        std::string format =
            "    " + std::string((*it)->Name()) + "\n"
            "    version: " + std::string((*it)->Version()) + "\n"
            "    by " + std::string((*it)->Author()) + "\n";

        this->WriteOutput(format, COLOR_PAIR(CURSESPP_TEXT_DEFAULT));
    }
}
