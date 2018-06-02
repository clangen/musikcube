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

#include "stdafx.h"
#include "HotkeysLayout.h"
#include <cursespp/Colors.h>
#include <app/model/HotkeysAdapter.h>

using namespace cursespp;
using namespace musik::cube;

using Entry = IScrollAdapter::EntryPtr;

HotkeysLayout::HotkeysLayout() {
    auto adapter = std::make_shared<HotkeysAdapter>();

    adapter->SetItemDecorator([this](ScrollableWindow*, size_t index, size_t, Entry) -> int64_t {
        if (this->listWindow->GetSelectedIndex() == index) {
            return COLOR_PAIR(CURSESPP_HIGHLIGHTED_LIST_ITEM);
        }
        return -1;
    });

    this->listWindow = std::make_shared<ListWindow>();
    this->listWindow->SetFrameVisible(true);
    this->listWindow->SetAdapter(adapter);
    this->listWindow->SetFrameTitle(_TSTR("hotkeys_title"));

    this->AddWindow(this->listWindow);
    this->listWindow->SetFocusOrder(0);
}

HotkeysLayout::~HotkeysLayout() {
}

void HotkeysLayout::SetShortcutsWindow(ShortcutsWindow* shortcuts) {

}

void HotkeysLayout::OnLayout() {
    this->listWindow->MoveAndResize(
        0, 0, this->GetWidth(), this->GetHeight());
}
