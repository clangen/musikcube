#pragma once

#include "stdafx.h"
#include "CommandWindow.h"
#include "Screen.h"
#include "Colors.h"

#include <core/debug.h>
#include <core/sdk/IPlugin.h>
#include <core/plugin/PluginFactory.h>
#include <core/library/track/TrackFactory.h>
#include <core/library/Indexer.h>

#include <boost/algorithm/string.hpp>

#define BUFFER_SIZE 2048
#define MAX_SIZE 2046

using musik::core::Indexer;
using musik::core::IndexerPtr;
using musik::core::LibraryFactory;
using musik::core::LibraryPtr;
using musik::core::TrackFactory;
using musik::core::TrackPtr;

template <class T>
bool tostr(T& t, const std::string& s) {
    std::istringstream iss(s);
    return !(iss >> t).fail();
}

CommandWindow::CommandWindow(Transport& transport, OutputWindow& output) {
    this->transport = &transport;
    this->buffer = new char[BUFFER_SIZE];
    this->bufferPosition = 0;
    this->SetSize(Screen::GetWidth() / 2, 3);
    this->SetPosition(0, Screen::GetHeight() - 3);
    this->Create();
    this->output = &output;
    this->paused = false;
    this->library = LibraryFactory::Libraries().at(0);
    this->output->WriteLine("type 'h' or 'help'\n", BOX_COLOR_BLACK_ON_GREY);
}

CommandWindow::~CommandWindow() {
    delete[] buffer;
}

void CommandWindow::WriteChar(int ch) {
    if (bufferPosition >= MAX_SIZE) {
        return;
    }

    waddch(this->GetContents(), ch);

    if (ch == 8) { /* backspace */
        wdelch(this->GetContents());

        if (bufferPosition > 0) {
            --bufferPosition;
        }
    }
    else if (ch == 10) { /* return */
        this->buffer[bufferPosition] = 0;
        std::string cmd(buffer);

        if (!this->ProcessCommand(cmd)) {
            if (cmd.size()) {
                output->WriteLine("> " + cmd, COLOR_PAIR(BOX_COLOR_BLACK_ON_GREY));
                output->WriteLine("illegal command: '" + cmd + "'\n", COLOR_PAIR(BOX_COLOR_RED_ON_GREY));
            }
        }
        else {
            output->WriteLine("> " + cmd + "\n", COLOR_PAIR(BOX_COLOR_GREEN_ON_BLACK));
        }

        wclear(this->GetContents());
        this->bufferPosition = 0;
    }
    else {
        this->buffer[bufferPosition++] = ch;
    }

    this->Repaint();
}

void CommandWindow::Seek(const std::vector<std::string>& args) {
    if (args.size() > 0) {
        double newPosition = 0;
        if (tostr<double>(newPosition, args[0])) {
            transport->SetPosition(newPosition);
        }
    }
}

void CommandWindow::SetVolume(const std::vector<std::string>& args) {
    if (args.size() > 0) {
        int newVolume = 0;
        if (tostr<int>(newVolume, args[0])) {
            this->SetVolume((float) newVolume / 100.0f);
        }
    }
}

void CommandWindow::SetVolume(float volume) {
    transport->SetVolume(volume);
}

void CommandWindow::Help() {
    int64 s = -1;
    this->output->WriteLine("\nhelp:\n", s);
    this->output->WriteLine("  <tab> to switch between windows\n", s);
    this->output->WriteLine("  pl [file]: play file at path", s);
    this->output->WriteLine("  pa: toggle pause/resume", s);
    this->output->WriteLine("  st: stop playing", s);
    //this->output->WriteLine("  ls: list currently playing", s);
    this->output->WriteLine("  lp: list loaded plugins", s);
    this->output->WriteLine("  v: <0 - 100>: set % volume", s);
    this->output->WriteLine("  sk <seconds>: seek to <seconds> into track", s);
    this->output->WriteLine("  addir <dir>: add a directory to be indexed", s);
    this->output->WriteLine("  rmdir <dir>: remove indexed directory path", s);
    this->output->WriteLine("  rescan: rescan metadata in index paths", s);
    this->output->WriteLine("\n  q: quit\n\n", s);
}

bool CommandWindow::ProcessCommand(const std::string& cmd) {
    std::vector<std::string> args;
    boost::algorithm::split(args, cmd, boost::is_any_of(" "));

    std::string name = args.size() > 0 ? args[0] : "";
    args.erase(args.begin());

    if (name == "plugins") {
        this->ListPlugins();
    }
    else if (name == "play" || name == "pl" || name == "p") {
        return this->PlayFile(args);
    }
    else if (name == "adddir") {
        std::string path = boost::algorithm::join(args, " ");
        library->Indexer()->AddPath(path);
    }
    else if (name == "rescan" || name == "scan" || name == "index") {
        library->Indexer()->RestartSync();
    }
    else if (name == "h" || name == "help") {
        this->Help();
    }
    else if (cmd == "pa" || cmd == "pause") {
        this->Pause();
    }
    else if (cmd == "s" || cmd =="stop") {
        this->Stop();
    }
    else if (name == "sk" || name == "seek") {
        this->Seek(args);
    }
    else if (name == "np" || name == "pl") {
        this->ListPlaying();
    }
    else if (name == "plugins") {
        this->ListPlugins();
    }
    else if (name == "v" || name == "volume") {
        this->SetVolume(args);
    }
    else if (name == "q" || name == "quit" || name == "exit") {
        /* */
    }
    else {
        return false;
    }


    return true;
}

bool CommandWindow::PlayFile(const std::vector<std::string>& args) {
    if (args.size() > 0) {
        std::string filename = boost::algorithm::join(args, " ");
        transport->Start(filename);
        return true;
    }

    return false;
}

void CommandWindow::Pause() {
    if (this->paused) {
        transport->Resume();
        this->paused = !this->paused;
    }
    else {
        transport->Pause();
        this->paused = !this->paused;
    }
}

void CommandWindow::Stop() {
    this->transport->Stop();
}

void CommandWindow::ListPlaying() {
    /*
    AudioStreamOverview overview = transport.StreamsOverview();
    AudioStreamOverviewIterator it;


    for (it = overview.begin(); it != overview.end(); ++it) {
    cout << *it << '\n';
    }

    cout << "------------------\n";
    cout << transport.NumOfStreams() << " playing" << std::std::endl;*/
}


void CommandWindow::ListPlugins() const {
    using musik::core::IPlugin;
    using musik::core::PluginFactory;

    typedef std::vector<boost::shared_ptr<IPlugin>> PluginList;
    typedef PluginFactory::NullDeleter<IPlugin> Deleter;

    PluginList plugins = PluginFactory::Instance()
        .QueryInterface<IPlugin, Deleter>("GetPlugin");

    PluginList::iterator it = plugins.begin();
    for (; it != plugins.end(); it++) {
        std::string format =
            "plugin:\n"
            "  name: " + std::string((*it)->Name()) + " "
            "v" + std::string((*it)->Version()) + "\n"
            "  author: " + std::string((*it)->Author()) + "\n";

        this->output->WriteLine(format, BOX_COLOR_RED_ON_BLUE);
    }
}