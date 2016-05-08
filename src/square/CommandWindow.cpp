#pragma once

#include "stdafx.h"
#include "CommandWindow.h"
#include "Screen.h"
#include "Colors.h"

#include <core/debug.h>
#include <core/sdk/IPlugin.h>
#include <core/plugin/PluginFactory.h>
#include <core/library/track/TrackFactory.h>
#include <core/library/LibraryFactory.h>
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

    this->ListPlugins();
    this->ListPlugins();
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
                output->WriteLine("illegal command: '" + cmd + "'");
            }
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
        float newVolume = 0;
        if (tostr<float>(newVolume, args[0])) {
            this->SetVolume(newVolume);
        }
    }
}

void CommandWindow::SetVolume(float volume) {
    transport->SetVolume(volume);
}

void CommandWindow::Help() {
    this->output->WriteLine("\n\nhelp:");
    this->output->WriteLine("  pl [file]: play file at path");
    this->output->WriteLine("  pa: toggle pause/resume");
    this->output->WriteLine("  st: stop playing");
    this->output->WriteLine("  ls: list currently playing");
    this->output->WriteLine("  lp: list loaded plugins");
    this->output->WriteLine("  v: <0.0-1.0>: set volume to p%");
    this->output->WriteLine("  sk <seconds>: seek to <seconds> into track");
    this->output->WriteLine("  rescan: rescan/index music directories");
    this->output->WriteLine("\n  q: quit\n\n");
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
    else if (name == "rescan" || name == "scan" || name == "index") {
        LibraryPtr library = LibraryFactory::Libraries().at(0); /* there's always at least 1... */
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
        transport->Start(args.at(0));
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

        this->output->WriteLine(format);
    }
}