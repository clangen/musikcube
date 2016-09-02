//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include "Overlays.h"
#include "DialogOverlay.h"
#include "Colors.h"
#include "Screen.h"

using namespace cursespp;

static ILayoutPtr none;

Overlays::Overlays() {
    std::shared_ptr<DialogOverlay> temp;

    temp.reset(new DialogOverlay());

    temp->SetTitle("musikbox")
        .SetMessage("welcome to musikbox! welcome to musikbox! welcome to musikbox! welcome to musikbox!\n\ntesting line breaks");

    temp->AddButton(
        "KEY_ENTER",
        "ENTER",
        "ok",
        [this](std::string kn) {

        });

    temp->AddButton(
        "^[",
        "ESC",
        "cancel",
        [this](std::string kn) {

        });

    this->Push(temp);
}

ILayoutPtr Overlays::Top() {
    return this->stack.size() ? this->stack[0] : none;
}

inline void setOverlays(ILayoutPtr layout, Overlays* instance) {
    IOverlay* overlay = dynamic_cast<IOverlay*>(layout.get());
    if (overlay) {
        overlay->SetOverlays(instance);
    }
}

void Overlays::Push(ILayoutPtr layout) {
    setOverlays(layout, this);
    this->stack.insert(this->stack.begin(), layout);
}

void Overlays::Remove(ILayoutPtr layout) {
    auto it = std::find(
        this->stack.begin(),
        this->stack.end(), layout);

    if (it != this->stack.end()) {
        this->stack.erase(it);
        setOverlays(*it, nullptr);
    }
}

void Overlays::Remove(ILayout* layout) {
    auto it = std::find_if(
        this->stack.begin(),
        this->stack.end(),
        [layout] (ILayoutPtr layoutPtr) {
            return layoutPtr.get() == layout;
        });

    if (it != this->stack.end()) {
        this->stack.erase(it);
        setOverlays(*it, nullptr);
    }
}