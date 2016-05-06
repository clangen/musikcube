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
#include "config.h"
#include <vector>
#include <sigslot/sigslot.h>

namespace musik { namespace square {

    class ConsoleUI;

    class TransportEvents : public sigslot::has_slots<> {
        public:    
            TransportEvents(ConsoleUI* c);
            ~TransportEvents();

            void OnPlaybackAlmostEnded() { this->LogEvent("about to end"); };
            void OnPlaybackStartedOk() { this->LogEvent("started"); };
            void OnPlaybackStartedFail() { this->LogEvent("failed"); };
            void OnPlaybackStoppedOk() { this->LogEvent("stopped"); };
            void OnPlaybackStoppedFail() { this->LogEvent("stopped because of failure"); };
            void OnPlaybackInterrupted() { this->LogEvent("interrupted (??)"); };
            void OnVolumeChangedOk() { this->LogEvent("volume changed"); };
            void OnStreamOpenOk() { this->LogEvent("stream open ok"); };
            void OnStreamOpenFail() { this->LogEvent("stream open fail"); };
            void OnMixpointReached() { this->LogEvent("mix point reached"); }
            void OnSetPositionOk() { this->LogEvent("set position"); };
            void OnSetPositionFail() { this->LogEvent("set position failed"); };
            void OnPlaybackPaused() { this->LogEvent("paused"); };
            void OnPlaybackResumed() { this->LogEvent("resumed"); };

        private:
            ConsoleUI* cui;
            void LogEvent(std::string s);
    };

} }
