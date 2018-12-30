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
#include <cursespp/OverlayStack.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/Colors.h>
#include <cursespp/Screen.h>

using namespace cursespp;

static ILayoutPtr none;

OverlayStack::OverlayStack() {
}

ILayoutPtr OverlayStack::Top() {
    return this->stack.size() ? this->stack[0] : none;
}

inline void setOverlayStack(ILayoutPtr layout, OverlayStack* instance) {
    IOverlay* overlay = dynamic_cast<IOverlay*>(layout.get());
    if (overlay) {
        overlay->SetOverlayStack(instance);
    }
}

void OverlayStack::Push(ILayoutPtr layout) {
    setOverlayStack(layout, this);

    auto it = std::find(
        this->stack.begin(),
        this->stack.end(), layout);

    if (it != this->stack.end()) {
        this->stack.erase(it); /* remove; we'll promote to the top */
    }

    this->stack.insert(this->stack.begin(), layout);
}

void OverlayStack::Remove(ILayoutPtr layout) {
    auto it = std::find(
        this->stack.begin(),
        this->stack.end(), layout);

    if (it != this->stack.end()) {
        setOverlayStack(*it, nullptr);
        this->stack.erase(it);
    }
}

void OverlayStack::Remove(ILayout* layout) {
    auto it = std::find_if(
        this->stack.begin(),
        this->stack.end(),
        [layout] (ILayoutPtr layoutPtr) {
            return layoutPtr.get() == layout;
        });

    if (it != this->stack.end()) {
        setOverlayStack(*it, nullptr);
        this->stack.erase(it);
    }
}

void OverlayStack::Clear() {
    this->stack.clear();
}