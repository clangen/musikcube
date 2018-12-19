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

#include "constants.h"
#include "SuperEqDsp.h"
#include <core/sdk/constants.h>
#include <core/sdk/IPreferences.h>
#include <core/sdk/ISchema.h>
#include <atomic>

using namespace musik::core::sdk;

static IPreferences* prefs = nullptr;
static std::atomic<int> currentState;

static const std::vector<std::string> BANDS = {
    "65", "92", "131", "185", "262",
    "370", "523", "740", "1047", "1480",
    "2093", "2960", "4186", "5920", "8372",
    "11840", "16744"
};

extern "C" DLLEXPORT void SetPreferences(IPreferences* prefs) {
    ::prefs = prefs;
}

void SuperEqDsp::NotifyChanged() {
    currentState.fetch_add(1);
}

SuperEqDsp::SuperEqDsp() {
    this->enabled = ::prefs && ::prefs->GetBool("enabled", false);
}

SuperEqDsp::~SuperEqDsp() {
    if (this->supereq) {
        equ_quit(this->supereq);
        delete this->supereq;
    }
}

void SuperEqDsp::Release() {
    delete this;
}

bool SuperEqDsp::Process(IBuffer* buffer) {
    int channels = buffer->Channels();
    int current = ::currentState.load();

    if (!this->supereq || this->lastUpdated != current) {
        this->enabled = ::prefs && ::prefs->GetBool("enabled", false);
        this->lastUpdated = current;

        this->supereq = new SuperEqState();
        equ_init(this->supereq, 10, channels);

        void *params = paramlist_alloc();
        float bands[17];

        for (int i = 0; i < BANDS.size(); i++) {
            bands[i] = prefs->GetDouble(BANDS[i].c_str(), 1.0);
        }

        equ_makeTable(
            this->supereq,
            bands,
            params,
            buffer->SampleRate());

        paramlist_free(params);
    }

    if (!this->enabled) {
        return false;
    }

    return equ_modifySamples_float(
        this->supereq,
        (char*) buffer->BufferPointer(),
        buffer->Samples() / channels,
        channels) != 0;
}