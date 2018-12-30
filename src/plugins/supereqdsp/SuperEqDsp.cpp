//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
//
// All rights reserved.
//
// Note: this musikcube plugin (supereq) is not distributed under a BSD
// license like most other project components because it consumes GPL2
// code.
//
//////////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//////////////////////////////////////////////////////////////////////////////

#include "constants.h"
#include "SuperEqDsp.h"
#include <core/sdk/constants.h>
#include <core/sdk/IPreferences.h>
#include <core/sdk/ISchema.h>
#include <atomic>
#include <math.h>

using namespace musik::core::sdk;

static IPreferences* prefs = nullptr;
static std::atomic<int> currentState;

static const std::vector<std::string> BANDS = {
    "65", "92", "131", "185", "262",
    "370", "523", "740", "1047", "1480",
    "2093", "2960", "4186", "5920", "8372",
    "11840", "16744", "22000",
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

        if (!this->supereq) {
            this->supereq = new SuperEqState();
            equ_init(this->supereq, 10, channels);
        }

        void *params = paramlist_alloc();
        float bands[18];

        for (size_t i = 0; i < BANDS.size(); i++) {
            double dB =  prefs->GetDouble(BANDS[i].c_str(), 0.0);
            double amp = pow(10, dB / 20.f);
            bands[i] = (float) amp;
        }

        equ_makeTable(
            this->supereq,
            bands,
            params,
            (float) buffer->SampleRate());

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