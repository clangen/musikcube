//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <stdafx.h>

#include "RemoteLibrarySettingsLayout.h"

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <core/support/PreferenceKeys.h>

using namespace musik;
using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace cursespp;

RemoteLibrarySettingsLayout::RemoteLibrarySettingsLayout(musik::core::ILibraryPtr library)
: LayoutBase()
, library(library) {
    this->SetFocusMode(FocusModeTerminating);
    this->InitializeWindows();
}

RemoteLibrarySettingsLayout::~RemoteLibrarySettingsLayout() {
}

void RemoteLibrarySettingsLayout::OnLayout() {
}

void RemoteLibrarySettingsLayout::InitializeWindows() {
    this->SetFrameVisible(false);

    // int order = 0;
    // this->browseList->SetFocusOrder(order++);
    // this->addedPathsList->SetFocusOrder(order++);

    // this->AddWindow(this->browseList);
    // this->AddWindow(this->addedPathsList);
}

void RemoteLibrarySettingsLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);
    if (visible) {
        this->LoadPreferences();
    }
}

void RemoteLibrarySettingsLayout::LoadPreferences() {
}
