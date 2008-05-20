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

#pragma once

#include "stdafx.h"

#include <vector>

#include <sigslot/sigslot.h>

namespace musik { namespace square {

class ConsoleUI;

enum AudioStreamEvent;

class DummyAudioEventHandler : public sigslot::has_slots<>
{
public:	DummyAudioEventHandler(ConsoleUI* c);
public:	~DummyAudioEventHandler();

private: ConsoleUI* cui; // TODO: should probably be interface
private: void PrintEvent(utfstring s);

// Slots
public: void    OnPlaybackStartedOk()       {   this->PrintEvent(_T("Playback started OK")); };
public: void    OnPlaybackStartedFail()     {   this->PrintEvent(_T("Playback started FAIL")); };
public: void    OnPlaybackStoppedOk()       {   this->PrintEvent(_T("Playback stopped OK")); };
public: void    OnPlaybackStoppedFail()     {   this->PrintEvent(_T("Playback stopped FAIL")); };
public: void    OnPlaybackInterrupted()     {   this->PrintEvent(_T("Playback interrupted")); };
public: void    OnVolumeChangedOk()         {   this->PrintEvent(_T("Volume changed OK")); };
public: void    OnVolumeChangedFail()       {   this->PrintEvent(_T("Volume changed FAIL")); };
public: void    OnStreamOpenOk()            {   this->PrintEvent(_T("Stream open OK")); };
public: void    OnStreamOpenFail()          {   this->PrintEvent(_T("Stream open FAIL")); };
public: void    OnMixpointReached()         ;
public: void    OnSetPositionOk()           {   this->PrintEvent(_T("Set position OK")); };
public: void    OnSetPositionFail()         {   this->PrintEvent(_T("Set position FAIL")); };
};

}} // NS
