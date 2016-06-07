#include "stdafx.h"

#include <cursespp/Screen.h>
#include <cursespp/Colors.h>
#include <cursespp/MessageQueue.h>
#include <cursespp/Message.h>

#include "CommandWindow.h"

#include <core/debug.h>
#include <core/sdk/IPlugin.h>
#include <core/plugin/PluginFactory.h>
#include <core/library/Indexer.h>
#include <core/library/track/Track.h>
#include <core/library/Indexer.h>

#include <boost/algorithm/string.hpp>

#define MAX_SIZE 2046

using musik::core::Indexer;
using musik::core::IndexerPtr;
using musik::core::LibraryFactory;
using musik::core::LibraryPtr;
using musik::core::TrackPtr;
using musik::core::QueryPtr;

using namespace musik::core::audio;
using namespace musik::box;
using namespace cursespp;

template <class T>
bool tostr(T& t, const std::string& s) {
    std::istringstream iss(s);
    return !(iss >> t).fail();
}

inline static void redrawContents(IWindow &window, const std::string& text) {
    WINDOW* c = window.GetContent();
    werase(c);
    wprintw(c, text.c_str());
    window.Repaint();
}

CommandWindow::CommandWindow(
    IWindow *parent,
    Transport& transport,
    LibraryPtr library,
    OutputWindow& output,
    LogWindow& logWindow)
: Window(parent) {
    this->SetContentColor(BOX_COLOR_WHITE_ON_BLACK);
    this->transport = &transport;
    this->library = library;
    this->bufferLength = 0;
    this->output = &output;
    this->logWindow = &logWindow;
    this->paused = false;
    this->Help();
}

CommandWindow::~CommandWindow() {
}

void CommandWindow::Show() {
    Window::Show();
    redrawContents(*this, buffer);
}

void CommandWindow::Focus() {

}

void removeUtf8Char(std::string& value) {
    std::string::iterator it = value.end();
    std::string::iterator start = value.begin();
    if (it != start) {
        utf8::prior(it, start);
        value = std::string(value.begin(), it);
    }
}

void CommandWindow::Write(const std::string& key) {
    if (key == "^H" || key == "^?" || key == "KEY_BACKSPACE") { /* backspace */
        removeUtf8Char(this->buffer);
    }
    else if (key == "^M") { /* return */
        output->WriteLine("> " + this->buffer + "\n", COLOR_PAIR(BOX_COLOR_BLACK_ON_GREY));

        if (!this->ProcessCommand(this->buffer)) {
            if (this->buffer.size()) {
                output->WriteLine(
                    "illegal command: '" +
                    this->buffer +
                    "'\n", COLOR_PAIR(BOX_COLOR_RED_ON_GREY));
            }
        }

        this->buffer = "";
    }
    else {
        /* one character at a time. if it's more than one character, we're
        dealing with an escape sequence and should not print it. */
        if (u8len(key) == 1) {
            this->buffer += key;
        }
    }

    this->bufferLength = u8len(buffer);

    redrawContents(*this, buffer);
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
            this->SetVolume(newVolume / 100.0f);
        }
    }
}

void CommandWindow::SetVolume(float volume) {
    transport->SetVolume(volume);
}

void CommandWindow::Help() {
    int64 s = -1;
    this->output->WriteLine("help:\n", s);
    this->output->WriteLine("  <tab> to switch between windows", s);
    this->output->WriteLine("  ALT+Q console view", s);
    this->output->WriteLine("  ALT+W library view", s);
    this->output->WriteLine("", s);
    this->output->WriteLine("  addir <dir>: add a music directory", s);
    this->output->WriteLine("  rmdir <dir>: remove a music directory", s);
    this->output->WriteLine("  lsdirs: list scanned directories", s);
    this->output->WriteLine("  rescan: rescan paths for new metadata", s);
    this->output->WriteLine("", s);
    this->output->WriteLine("  play <uri>: play audio at <uri>", s);
    this->output->WriteLine("  pause: pause/resume", s);
    this->output->WriteLine("  stop: stop and clean up everything", s);
    this->output->WriteLine("  volume: <0 - 100>: set % volume", s);
    this->output->WriteLine("  clear: clear the log window", s);
    this->output->WriteLine("  seek <seconds>: seek to <seconds> into track", s);
    this->output->WriteLine("", s);
    this->output->WriteLine("  plugins: list loaded plugins", s);
    this->output->WriteLine("", s);
    this->output->WriteLine("  <ctrl+d>: quit\n", s);
}

bool CommandWindow::ProcessCommand(const std::string& cmd) {
    std::vector<std::string> args;
    boost::algorithm::split(args, cmd, boost::is_any_of(" "));

    std::string name = args.size() > 0 ? args[0] : "";
    args.erase(args.begin());

    if (name == "plugins") {
        this->ListPlugins();
    }
    else if (name == "clear") {
        this->logWindow->ClearContents();
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
        this->output->WriteLine("paths:");
        for (size_t i = 0; i < paths.size(); i++) {
            this->output->WriteLine("  " + paths.at(i));
        }
        this->output->WriteLine("");
    }
    else if (name == "rescan" || name == "scan" || name == "index") {
        library->Indexer()->Synchronize();
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
    else if (name == "plugins") {
        this->ListPlugins();
    }
    else if (name == "v" || name == "volume") {
        this->SetVolume(args);
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

void CommandWindow::ListPlugins() const {
    using musik::core::IPlugin;
    using musik::core::PluginFactory;

    typedef std::vector<std::shared_ptr<IPlugin> > PluginList;
    typedef PluginFactory::NullDeleter<IPlugin> Deleter;

    PluginList plugins = PluginFactory::Instance()
        .QueryInterface<IPlugin, Deleter>("GetPlugin");

    PluginList::iterator it = plugins.begin();
    for (; it != plugins.end(); it++) {
        std::string format =
            "    " + std::string((*it)->Name()) + " "
            "v" + std::string((*it)->Version()) + "\n"
            "    by " + std::string((*it)->Author()) + "\n";

        this->output->WriteLine(format, BOX_COLOR_RED_ON_BLUE);
    }
}
