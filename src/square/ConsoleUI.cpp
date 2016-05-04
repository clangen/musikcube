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

#include <boost/algorithm/string.hpp>

#include <core/sdk/IPlugin.h>
#include <core/plugin/PluginFactory.h>

#include <core/library/track/TrackFactory.h>
#include <core/library/LibraryFactory.h>
#include <core/library/Indexer.h>

using namespace musik::square;

using std::cout;
using std::cin;

ConsoleUI::ConsoleUI() 
: shouldQuit(false)
, audioEventHandler(this)
, paused(false)
{
//    this->transport.EventPlaybackStartedOk.connect(&audioEventHandler, &DummyAudioEventHandler::OnPlaybackStartedOk);
//    this->transport.EventPlaybackStartedFail.connect(&audioEventHandler, &DummyAudioEventHandler::OnPlaybackStartedFail);

//    this->transport.EventPlaybackStoppedOk.connect(&audioEventHandler, &DummyAudioEventHandler::OnPlaybackStoppedOk);
//    this->transport.EventPlaybackStoppedFail.connect(&audioEventHandler, &DummyAudioEventHandler::OnPlaybackStoppedFail);

 //   this->transport.EventVolumeChangedOk.connect(&audioEventHandler, &DummyAudioEventHandler::OnVolumeChangedOk);
 //   this->transport.EventVolumeChangedFail.connect(&audioEventHandler, &DummyAudioEventHandler::OnVolumeChangedFail);

 //   this->transport.EventMixpointReached.connect(&audioEventHandler, &DummyAudioEventHandler::OnMixpointReached);
 	
	this->transport.PlaybackStarted.connect(&this->audioEventHandler, &DummyAudioEventHandler::OnPlaybackStartedOk);
	this->transport.PlaybackAlmostDone.connect(&this->audioEventHandler, &DummyAudioEventHandler::OnPlaybackAlmostEnded);
	this->transport.PlaybackEnded.connect(&this->audioEventHandler, &DummyAudioEventHandler::OnPlaybackStoppedOk);
	this->transport.PlaybackError.connect(&this->audioEventHandler, &DummyAudioEventHandler::OnPlaybackStoppedFail);
}

ConsoleUI::~ConsoleUI() {
}

void ConsoleUI::Print(std::string s)
{
    boost::mutex::scoped_lock   lock(mutex);

	cout << "\n*******************************\n\n";
    cout << s;
    cout << "\n*******************************\n" << std::endl;
}

void ConsoleUI::Run()
{
    std::string command; 

    transport.SetVolume(0.1);

    using musik::core::Indexer;
    using musik::core::IndexerPtr;
    using musik::core::LibraryFactory;
    using musik::core::LibraryPtr;
    using musik::core::TrackFactory;
    using musik::core::TrackPtr;
   
    LibraryPtr library = LibraryFactory::Libraries().at(0); /* there's always at least 1... */
    library->Indexer()->AddPath("E:/music/ripped/MR_Bungle");
    library->Indexer()->RestartSync();

    while (!this->shouldQuit) {
        this->PrintCommands();
        cout << "Enter command: ";
        std::getline(cin, command); // Need getline to handle spaces!
        this->ProcessCommand(command);
    }
}

void ConsoleUI::PrintCommands()
{
    boost::mutex::scoped_lock   lock(mutex);

    cout << "Commands:\n";
    cout << "\tp [file]: play file (enter full path)\n";
    cout << "\ts: stop playing\n";
    cout << "\tl: list currently playing\n";
    cout << "\tlp: list loaded plugins\n";
    cout << "\tv <p>: set volume to p%\n";
    cout << "\tq: quit";
    cout << std::endl;
}

void ConsoleUI::ProcessCommand(std::string commandString)
{
    Args args;
        
    boost::algorithm::split(args, commandString, boost::is_any_of(" "));

    std::string command = args.size() > 0 ? args[0] : "";
    args.erase(args.begin());

    if (command == "p")
    {
        this->PlayFile(args);
    }
    else if (command == "pa")
    {
        this->Pause();
    }
    else if (command == "s")
    {
        this->Stop(args);
    }
    else if (command == "seek")
    {
    	this->SetPosition(args);
    }
    else if (command == "l")
    {
        this->ListPlaying();
    }
    else if (command == "lp")
    {
        this->ListPlugins();
    }
    else if (command == "v")
    {
        this->SetVolume(args);
    }
    else if (command == "q")
    {
        this->Quit();
    }
    else
    {
        cout << "Unknown command\n";
    }
}

void ConsoleUI::PlayFile(Args args)
{
    std::string filename;
    if (args.size() > 0) 
    {
        filename = args[0];
    }

    int repeat = 1;
    if (args.size() > 1)
    {
        convertString<int>(repeat, args[1]);
    }

    unsigned int delay = 0;
    if (args.size() > 2)
    {        
        if (!convertString<unsigned int>(delay, args[2]))
        {
            delay = 0;
        }
    }

    for (int i = 0; i < repeat; ++i)
    {
        transport.Start(filename.c_str()); //TODO: fix to use TrackPtr
        if (delay)
        {
#ifdef WIN32
			Sleep(delay);
#else
            sleep(delay);
#endif
        }
    }
}

void ConsoleUI::Pause()
{
	if (this->paused) {
		transport.Resume();
		this->paused = !this->paused;
	}
	else
	{
		transport.Pause();
		this->paused = !this->paused;
	}
}

void ConsoleUI::Stop(Args args)
{
    this->Stop();
}

void ConsoleUI::Stop()
{
    transport.Stop();
}

void ConsoleUI::ListPlaying()
{
	/*
	AudioStreamOverview overview = transport.StreamsOverview();
	AudioStreamOverviewIterator it;
	

	for (it = overview.begin(); it != overview.end(); ++it)
	{
		cout << *it << '\n';
	}
    
	cout << "------------------\n";
	cout << transport.NumOfStreams() << " playing" << std::std::endl;*/
}

void ConsoleUI::ListPlugins()
{
    using musik::core::IPlugin;
    using musik::core::PluginFactory;

    typedef std::vector<boost::shared_ptr<IPlugin> > PluginList;
    typedef PluginFactory::NullDeleter<IPlugin> Deleter;

    PluginList plugins = 
        PluginFactory::Instance().QueryInterface<IPlugin, Deleter>("GetPlugin");

    PluginList::iterator it = plugins.begin();
    for ( ; it != plugins.end(); it++)
    {
        cout << (*it)->Name() << '\t' << (*it)->Version() << '\t' << (*it)->Author() << '\n';
    }
}

void ConsoleUI::SetPosition(Args args)
{
	if (args.size() > 0) {
		double newPosition = 0;
		if (convertString<double>(newPosition, args[0])) {
			transport.SetPosition(newPosition);
		}
	}
}

void ConsoleUI::SetVolume(Args args)
{
    if (args.size() > 0)
    {
        float newVolume = 0;
        if (convertString<float>(newVolume, args[0]))
        {
            this->SetVolume(newVolume);
        }
    }
}

void ConsoleUI::SetVolume(float volume)
{
    transport.SetVolume(volume);
}

void ConsoleUI::Quit()
{   
    this->shouldQuit = true;
}

void ConsoleUI::ShutDown()
{
}

#ifdef WIN32
/*static*/ DWORD WINAPI ConsoleUI::ThreadRun(LPVOID param)
{
    ConsoleUI* instance = (ConsoleUI*)param;
    instance->Run();
    delete instance;
    return 0;
}
#endif

void ConsoleUI::StartNew()
{
    Args a;
    this->PlayFile(a);
}
