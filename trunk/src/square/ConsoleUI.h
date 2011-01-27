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

// This class provides a basic console UI to make testing the audio engine easier

#pragma once

#include "stdafx.h"

#include <vector>

#include <boost/thread/mutex.hpp>

#include <core/audio/Transport.h>

#include "DummyAudioEventHandler.h"

using namespace musik::core::audio;

class AudioStream;

namespace musik { namespace square {

typedef std::vector<utfstring> Args;

class ConsoleUI
{
public:  ConsoleUI();
public: ~ConsoleUI();

public:  void        Run();
private: void        PrintCommands();
private: void        ProcessCommand(utfstring commandString);
public:  void        Print(utfstring s);

// Commands
private: void   PlayFile(Args args);
private: void   Stop(Args args);
private: void   Stop();
private: void   ListPlaying();
private: void   ListPlugins();
private: void   SetVolume(Args args);
private: void   SetVolume(short volume);
private: void   Quit();

// Members
private: bool                       shouldQuit;
private: Transport                  transport;
private: DummyAudioEventHandler     audioEventHandler;

private: boost::mutex   mutex;

private: void ShutDown();

#ifdef WIN32
public: static DWORD WINAPI ThreadRun(LPVOID param);
#endif

public: void StartNew();
};

}} // NS

// TODO: move to utility file
#include <sstream>

template <class T>
bool convertString(T& t, const utfstring& s)
{
    std::utfistringstream iss(s);
    
    return !(iss >> t).fail();
}
