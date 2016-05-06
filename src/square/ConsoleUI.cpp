//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, Björn Olievier
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

#include "ConsoleUI.h"

#include <iostream>
#include <sstream>

#include <boost/algorithm/string.hpp>
#include <boost/thread/mutex.hpp>

#include <core/debug.h>
#include <core/sdk/IPlugin.h>
#include <core/plugin/PluginFactory.h>
#include <core/library/track/TrackFactory.h>
#include <core/library/LibraryFactory.h>
#include <core/library/Indexer.h>

template <class T>
bool tostr(T& t, const std::string& s) {
    std::istringstream iss(s);
    return !(iss >> t).fail();
}

using namespace musik::square;

using std::cout;
using std::cin;
using musik::core::Indexer;
using musik::core::IndexerPtr;
using musik::core::LibraryFactory;
using musik::core::LibraryPtr;
using musik::core::TrackFactory;
using musik::core::TrackPtr;

static boost::mutex outputMutex;

typedef boost::mutex::scoped_lock Lock;
static Lock lockOutput() {
    return Lock(outputMutex);
}

ConsoleUI::ConsoleUI() 
: shouldQuit(false)
, audioEventHandler(this)
, paused(false) {

    musik::debug::string_logged.connect(this, &ConsoleUI::OnDebug);

//    this->transport.EventPlaybackStopped.connect(&audioEventHandler, &TransportEvents::OnPlaybackStopped);
//    this->transport.EventVolumeChanged.connect(&audioEventHandler, &TransportEvents::OnVolumeChanged);
//    this->transport.EventMixpointReached.connect(&audioEventHandler, &TransportEvents::OnMixpointReached);
    this->transport.PlaybackStarted.connect(&this->audioEventHandler, &TransportEvents::OnPlaybackStartedOk);
    this->transport.PlaybackAlmostDone.connect(&this->audioEventHandler, &TransportEvents::OnPlaybackAlmostEnded);
    this->transport.PlaybackEnded.connect(&this->audioEventHandler, &TransportEvents::OnPlaybackStoppedOk);
    this->transport.PlaybackError.connect(&this->audioEventHandler, &TransportEvents::OnPlaybackStoppedFail);
    this->transport.PlaybackPause.connect(&this->audioEventHandler, &TransportEvents::OnPlaybackPaused);
    this->transport.PlaybackResume.connect(&this->audioEventHandler, &TransportEvents::OnPlaybackResumed);

}

void ConsoleUI::OnDebug(musik::debug::log_level level, std::string tag, std::string message) {
    std::cout << "    [" << tag << "] " << message << "\n";
}

ConsoleUI::~ConsoleUI() {
}

void ConsoleUI::Run() {
    std::string cmd; 

    transport.SetVolume(0.1);
   
    LibraryPtr library = LibraryFactory::Libraries().at(0); /* there's always at least 1... */
    library->Indexer()->AddPath("E:/music/");

    this->Help();

    while (!this->shouldQuit) {
        cout << "> ";
        std::getline(cin, cmd);
        this->Process(cmd);
    }
}

void ConsoleUI::Help() {
    Lock lock = lockOutput();

    cout << "help:\n";
    cout << "  pl [file]: play file at path\n";
    cout << "  pa: toggle pause/resume";
    cout << "  st: stop playing\n";
    cout << "  ls: list currently playing\n";
    cout << "  lp: list loaded plugins\n";
    cout << "  v: <0.0-1.0>: set volume to p%\n";
    cout << "  sk <seconds>: seek to <seconds> into track\n";
    cout << "  rescan: rescan/index music directories\n";
    cout << "\n  q: quit\n\n";
}

void ConsoleUI::Process(const std::string& cmd)
{
    Args args;
        
    boost::algorithm::split(args, cmd, boost::is_any_of(" "));

    std::string command = args.size() > 0 ? args[0] : "";
    args.erase(args.begin());

    if (command == "rmdir") {

    }
    else if (command == "addir") {

    }
    else if (command == "dirs") {

    }
    else if (command == "rescan" || command == "scan" || command == "index") {
        LibraryPtr library = LibraryFactory::Libraries().at(0); /* there's always at least 1... */
        library->Indexer()->RestartSync();
    }
    else if (command == "h" || command == "help") {
        this->Help();
    }
    else if (command == "p" || command == "pl") {
        this->PlayFile(args);
    }
    else if (command == "pa") {
        this->Pause();
    }
    else if (command == "s") {
        this->Stop(args);
    }
    else if (command == "sk" || command == "seek") {
        this->SetPosition(args);
    }
    else if (command == "np" || command == "pl") {
        this->ListPlaying();
    }
    else if (command == "plugins") {
        this->ListPlugins();
    }
    else if (command == "v" || command == "volume") {
        this->SetVolume(args);
    }
    else if (command == "q" || command == "quit" || command == "exit") {
        this->Quit();
    }
    else {
        cout << "\n\nunknown command ('h' for help)\n\n";
    }
}

void ConsoleUI::PlayFile(Args args) {
    std::string filename;
    if (args.size() > 0)  {
        filename = args[0];
    }

    int repeat = 1;
    if (args.size() > 1) {
        tostr<int>(repeat, args[1]);
    }

    unsigned int delay = 0;
    if (args.size() > 2) {
        if (!tostr<unsigned int>(delay, args[2])) {
            delay = 0;
        }
    }

    for (int i = 0; i < repeat; ++i) {
        transport.Start(filename.c_str()); //TODO: fix to use TrackPtr
        if (delay) {
#ifdef WIN32
            Sleep(delay);
#else
            sleep(delay);
#endif
        }
    }
}

void ConsoleUI::Pause() {
    if (this->paused) {
        transport.Resume();
        this->paused = !this->paused;
    }
    else {
        transport.Pause();
        this->paused = !this->paused;
    }
}

void ConsoleUI::Stop(Args args) {
    this->Stop();
}

void ConsoleUI::Stop() {
    transport.Stop();
}

void ConsoleUI::ListPlaying() {
    /*
    AudioStreamOverview overview = transport.StreamsOverview();
    AudioStreamOverviewIterator it;
    

    for (it = overview.begin(); it != overview.end(); ++it) {
        cout << *it << '\n';
    }
    
    cout << "------------------\n";
    cout << transport.NumOfStreams() << " playing" << std::std::endl;*/
}

void ConsoleUI::ListPlugins() {
    using musik::core::IPlugin;
    using musik::core::PluginFactory;

    typedef std::vector<boost::shared_ptr<IPlugin> > PluginList;
    typedef PluginFactory::NullDeleter<IPlugin> Deleter;

    PluginList plugins = PluginFactory::Instance()
        .QueryInterface<IPlugin, Deleter>("GetPlugin");

    PluginList::iterator it = plugins.begin();
    for ( ; it != plugins.end(); it++) {
        cout << (*it)->Name() << '\t' << 
            (*it)->Version() << '\t' << 
            (*it)->Author() << '\n';
    }
}

void ConsoleUI::SetPosition(Args args) {
    if (args.size() > 0) {
        double newPosition = 0;
        if (tostr<double>(newPosition, args[0])) {
            transport.SetPosition(newPosition);
        }
    }
}

void ConsoleUI::SetVolume(Args args) {
    if (args.size() > 0) {
        float newVolume = 0;
        if (tostr<float>(newVolume, args[0])) {
            this->SetVolume(newVolume);
        }
    }
}

void ConsoleUI::SetVolume(float volume) {
    transport.SetVolume(volume);
}

void ConsoleUI::Quit() {
    this->shouldQuit = true;
}

void ConsoleUI::ShutDown() {
}

#ifdef WIN32
/*static*/ DWORD WINAPI ConsoleUI::ThreadRun(LPVOID param) {
    ConsoleUI* instance = (ConsoleUI*) param;
    instance->Run();
    delete instance;
    return 0;
}
#endif
