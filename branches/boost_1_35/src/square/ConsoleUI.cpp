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

#include <core/IPlugin.h>
#include <core/PluginFactory.h>

using namespace std;
using namespace musik::square;

ConsoleUI::ConsoleUI() 
: shouldQuit(false)
, audioEventHandler(this)
{
    this->transport.PlaybackStartedOk.connect(&audioEventHandler, &DummyAudioEventHandler::OnPlaybackStartedOk);
    this->transport.PlaybackStartedFail.connect(&audioEventHandler, &DummyAudioEventHandler::OnPlaybackStartedFail);

    this->transport.PlaybackStoppedOk.connect(&audioEventHandler, &DummyAudioEventHandler::OnPlaybackStoppedOk);
    this->transport.PlaybackStoppedFail.connect(&audioEventHandler, &DummyAudioEventHandler::OnPlaybackStoppedFail);

    this->transport.VolumeChangedOk.connect(&audioEventHandler, &DummyAudioEventHandler::OnVolumeChangedOk);
    this->transport.VolumeChangedFail.connect(&audioEventHandler, &DummyAudioEventHandler::OnVolumeChangedFail);

    this->transport.MixpointReached.connect(&audioEventHandler, &DummyAudioEventHandler::OnMixpointReached);
}

ConsoleUI::~ConsoleUI()
{
}

void ConsoleUI::Print(utfstring s)
{
    boost::mutex::scoped_lock   lock(mutex);

	utfcout << "\n*******************************\n\n";
    utfcout << s;
    utfcout << "\n*******************************\n" << endl;
}

void ConsoleUI::Run()
{
    utfstring command; 

    while (!this->shouldQuit)
    {
        this->PrintCommands();
        utfcout << UTF("Enter command: ");
        std::getline(utfcin, command); // Need getline to handle spaces!
        this->ProcessCommand(command);
    }
}

void ConsoleUI::PrintCommands()
{
    boost::mutex::scoped_lock   lock(mutex);

    utfcout << "Commands:\n";
    utfcout << "\tp [file]: play file (enter full path)\n";
    utfcout << "\ts [n]: stop playing n-th file\n";
    utfcout << "\tl: list currently playing\n";
    utfcout << "\tlp: list loaded plugins\n";
    utfcout << "\tv <p>: set volume to p%\n";
    utfcout << "\tq: quit";
    utfcout << endl;
}

void ConsoleUI::ProcessCommand(utfstring commandString)
{
    using namespace boost::algorithm;

    Args args;
        
    split(args, commandString, is_any_of(" "));

    utfstring command = args.size() > 0 ? args[0] : _T("");
    args.erase(args.begin());

    if (command == _T("p"))
    {
        this->PlayFile(args);
    }
    else if (command == _T("s"))
    {
        this->Stop(args);
    }
    else if (command == _T("l"))
    {
        this->ListPlaying();
    }
    else if (command == _T("lp"))
    {
        this->ListPlugins();
    }
    else if (command == _T("v"))
    {
        this->SetVolume(args);
    }
    else if (command == _T("q"))
    {
        this->Quit();
    }
    else
    {
        utfcout << "Unknown command\n";
    }
}

void ConsoleUI::PlayFile(Args args)
{
    utfstring filename;
    if (args.size() > 0) 
    {
        filename = args[0];
    }
    else
    {
        filename = _T("C:\\temp\\musik\\ding.mp3"); //TODO: remove.  For quick testing only
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
        transport.Start(filename.c_str());
        if (delay)
        {
            Sleep(delay);
        }
    }
}

void ConsoleUI::Stop(Args args)
{
    size_t  idx;

    if (args.size() == 0)
    {
        this->Stop(0);
    }
    else 
    {
        if (convertString<size_t>(idx, args[0]))
        {
           this->Stop(idx);
        }
    }
}

void ConsoleUI::Stop(size_t idx)
{
    transport.Stop(idx);
}

void ConsoleUI::ListPlaying()
{
	AudioStreamOverview overview = transport.StreamsOverview();
	AudioStreamOverviewIterator it;

	for (it = overview.begin(); it != overview.end(); ++it)
	{
		utfcout << *it << '\n';
	}
    
	utfcout << "------------------\n";
	utfcout << transport.NumOfStreams() << " playing" << endl;
}

void ConsoleUI::ListPlugins()
{
    using musik::core::IPlugin;
    using musik::core::PluginFactory;

    typedef std::vector<boost::shared_ptr<IPlugin>> PluginList;
    typedef PluginFactory::NullDeleter<IPlugin> Deleter;

    PluginList plugins = 
        PluginFactory::Instance().QueryInterface<IPlugin, Deleter>("GetPlugin");

    PluginList::iterator it = plugins.begin();
    for ( ; it != plugins.end(); it++)
    {
        utfcout << (*it)->Name() << '\t' << (*it)->Version() << '\t' << (*it)->Author() << '\n';
    }
}

void ConsoleUI::SetVolume(Args args)
{
    if (args.size() > 0)
    {
        short newVolume = 0;
        if (convertString<short>(newVolume, args[0]))
        {
            this->SetVolume(newVolume);
        }
    }
}

void ConsoleUI::SetVolume(short volume)
{
    transport.ChangeVolume(volume);
}

void ConsoleUI::Quit()
{   
    this->shouldQuit = true;
}

void ConsoleUI::ShutDown()
{
}

/*static*/ DWORD WINAPI ConsoleUI::ThreadRun(LPVOID param)
{
    ConsoleUI* instance = (ConsoleUI*)param;
    instance->Run();
    delete instance;
    return 0;
}

void ConsoleUI::StartNew()
{
    Args a;
    this->PlayFile(a);
}