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

#include <stdafx.h>
#include "LayoutStack.h"

using namespace cursespp;

LayoutStack::LayoutStack() {
    this->visible = false;
    this->stack = 0;
}

LayoutStack::~LayoutStack() {

}

static inline bool contains(const std::deque<ILayoutPtr>& group, ILayoutPtr ptr) {
    for (size_t i = 0; i < group.size(); i++) {
        if (group.at(i) == ptr) {
            return true;
        }
    }

    return false;
}

static inline bool erase(std::deque<ILayoutPtr>& group, ILayoutPtr ptr) {
    std::deque<ILayoutPtr>::iterator it = group.begin();
    while (it != group.end()) {
        if (*it == ptr) {
            group.erase(it);
            return true;
        }
        else {
            ++it;
        }
    }

    return false;
}

bool LayoutStack::AddWindow(IWindowPtr window) {
    throw std::runtime_error("AddWindow() not supported in LayoutStack. Use Push()");
}

bool LayoutStack::RemoveWindow(IWindowPtr window) {
    throw std::runtime_error("RemoveWindow() not supported in LayoutStack. Use Push()");
}

size_t LayoutStack::GetWindowCount() {
    throw std::runtime_error("GetWindowCount() not supported in LayoutStack.");
}

IWindowPtr LayoutStack::GetWindowAt(size_t position) {
    throw std::runtime_error("GetWindowAt() not supported in LayoutStack.");
}

ILayoutStack* LayoutStack::GetLayoutStack() {
    return this->stack;
}

void LayoutStack::SetLayoutStack(ILayoutStack* stack) {
    this->stack = stack;
}

void LayoutStack::Show() {
    if (!this->visible) {
        std::deque<ILayoutPtr>::iterator it = this->layouts.begin();
        while (it != this->layouts.end()) {
            (*it)->Show();
            ++it;
        }

        this->visible = true;
    }

    this->BringToTop();
}

void LayoutStack::Hide() {
    if (this->visible) {
        std::deque<ILayoutPtr>::iterator it = this->layouts.begin();
        while (it != this->layouts.end()) {
            (*it)->Hide();
            ++it;
        }

        this->visible = false;
    }
}

bool LayoutStack::KeyPress(const std::string& key) {
    if (!this->layouts.size()) {
        return false;
    }

    return this->layouts.front()->KeyPress(key);
}

IWindowPtr LayoutStack::FocusNext() {
    if (!this->layouts.size()) {
        return IWindowPtr();
    }

    return this->layouts.front()->FocusNext();
}

ILayoutPtr LayoutStack::Top() {
    if (this->layouts.size()) {
        return this->layouts.front();
    }

    return ILayoutPtr();
}

IWindowPtr LayoutStack::FocusPrev() {
    if (!this->layouts.size()) {
        return IWindowPtr();
    }

    return this->layouts.front()->FocusPrev();
}

IWindowPtr LayoutStack::GetFocus() {
    if (!this->layouts.size()) {
        return IWindowPtr();
    }

    return this->layouts.front()->GetFocus();
}

bool LayoutStack::Push(ILayoutPtr layout) {
    ILayoutStack* oldStack = layout->GetLayoutStack();

    if (oldStack && oldStack != this) {
        oldStack->Pop(layout);
    }

    layout->SetLayoutStack(this);

    if (!contains(this->layouts, layout)) {
        this->layouts.push_front(layout);
    }

    this->BringToTop();
    return true;
}

bool LayoutStack::Pop(ILayoutPtr layout) {
    bool erased = erase(this->layouts, layout);

    if (erased) {
        layout->Hide();
        this->BringToTop();
        return true;
    }

    return false;
}

void LayoutStack::BringToTop() {
    std::deque<ILayoutPtr>::reverse_iterator it = this->layouts.rbegin();
    while (it != this->layouts.rend()) {
        (*it)->BringToTop();
        ++it;
    }
}

void LayoutStack::SendToBottom() {
    std::deque<ILayoutPtr>::reverse_iterator it = this->layouts.rbegin();
    while (it != this->layouts.rend()) {
        (*it)->SendToBottom();
        ++it;
    }
}

bool LayoutStack::BringToTop(ILayoutPtr layout) {
    if (this->layouts.front() != layout) {
        if (!erase(this->layouts, layout)) {
            return false;
        }
    }

    this->layouts.push_front(layout);
    this->BringToTop();
    return true;
}

bool LayoutStack::SendToBottom(ILayoutPtr layout) {
    if (this->layouts.back() != layout) {
        if (!erase(this->layouts, layout)) {
            return false;
        }
    }

    this->layouts.push_back(layout);
    this->SendToBottom();
    return true;
}
