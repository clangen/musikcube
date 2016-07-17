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
#include "LayoutBase.h"
#include "LayoutStack.h"
#include "Colors.h"

using namespace cursespp;

template <typename T> static int find(std::vector<T>& haystack, T& needle) {
    int i = 0;
    typename std::vector<T>::iterator it = haystack.begin();
    for (; it != haystack.end(); it++, i++) {
        if ((*it) == needle) {
            return i;
        }
    }

    return -1;
}

bool sortByFocusOrder(IWindowPtr a, IWindowPtr b) {
    int orderA = a->GetFocusOrder();
    int orderB = b->GetFocusOrder();

    if (orderA == orderB) {
        return a->GetId() < b->GetId();
    }

    return orderA < orderB;
}

static inline IWindowPtr adjustFocus(IWindowPtr oldFocus, IWindowPtr newFocus) {
    if (oldFocus) {
        oldFocus->SetFrameColor(CURSESPP_DEFAULT_FRAME_COLOR);
        oldFocus->Blur();
    }

    if (newFocus) {
        newFocus->SetFrameColor(CURSESPP_FOCUSED_FRAME_COLOR);
        newFocus->Focus();
    }

    return newFocus;
}

LayoutBase::LayoutBase(IWindow* parent)
: Window(parent) {
    this->focused = -1;
    this->layoutStack = 0;
    this->SetFrameVisible(false);
}

LayoutBase::~LayoutBase() {

}

void LayoutBase::Show() {
    Window::Show();

    for (size_t i = 0; i < this->children.size(); i++) {
        this->children.at(i)->Show();
    }

    this->IndexFocusables();
    this->SortFocusables();
}

void LayoutBase::Hide() {
    for (size_t i = 0; i < this->children.size(); i++) {
        this->children.at(i)->Hide();
    }

    Window::Hide();
}

void LayoutBase::BringToTop() {
    Window::BringToTop();

    for (size_t i = 0; i < this->children.size(); i++) {
        this->children.at(i)->BringToTop();
    }

    this->Repaint();
}

void LayoutBase::SendToBottom() {
    for (size_t i = 0; i < this->children.size(); i++) {
        this->children.at(i)->SendToBottom();
    }

    Window::SendToBottom();
}

void LayoutBase::Repaint() {
    /* repaint bottom up. start with ourselves, then our children,
    recursively. */

    Window::Repaint();

    for (size_t i = 0; i < this->children.size(); i++) {
        this->children.at(i)->Repaint();
    }
}

bool LayoutBase::AddWindow(IWindowPtr window) {
    if (find(this->children, window) >= 0) {
        return true;
    }

    window->SetParent(this);

    this->children.push_back(window);
    AddFocusable(window);

    return true;
}

bool LayoutBase::RemoveWindow(IWindowPtr window) {
    this->RemoveFocusable(window);

    std::vector<IWindowPtr>::iterator it = this->children.begin();
    for ( ; it != this->children.end(); it++) {
        if (*it == window) {
            this->children.erase(it);
            return true;
        }
    }

    return false;
}

void LayoutBase::AddFocusable(IWindowPtr window) {
    int order = window->GetFocusOrder();
    if (order >= 0 && find(this->focusable, window) < 0) {
        this->focusable.push_back(window);
        this->SortFocusables();
    }
}

void LayoutBase::IndexFocusables() {
    IWindowPtr focusedWindow;
    if (focused >= 0 && (int) this->focusable.size() > focused) {
        focusedWindow = this->focusable.at(focused);
    }

    this->focusable.clear();
    for (size_t i = 0; i < this->children.size(); i++) {
        AddFocusable(this->children.at(i));
    }

    if (focusedWindow) {
        this->focused = find(this->focusable, focusedWindow);
    }
}

void LayoutBase::SortFocusables() {
    IWindowPtr focusedWindow;
    if (focused >= 0 && (int) this->focusable.size() > focused) {
        focusedWindow = this->focusable.at(focused);
    }

    std::sort(
        this->focusable.begin(),
        this->focusable.end(),
        sortByFocusOrder);

    if (focusedWindow) {
        this->focused = find(this->focusable, focusedWindow);
    }

    if (focused == -1 && this->focusable.size() > 0) {
        this->focused = 0;
        adjustFocus(IWindowPtr(), this->focusable[this->focused]);
    }
}

void LayoutBase::RemoveFocusable(IWindowPtr window) {
    std::vector<IWindowPtr>::iterator it = this->focusable.begin();
    for (; it != this->focusable.end(); it++) {
        if (*it == window) {
            this->focusable.erase(it);
            return;
        }
    }
}

size_t LayoutBase::GetWindowCount() {
    return this->children.size();
}

IWindowPtr LayoutBase::GetWindowAt(size_t position) {
    return this->children.at(position);
}

bool LayoutBase::SetFocus(IWindowPtr focus) {
    if (!focus) {
        adjustFocus(GetFocus(), focus);
        this->focused = -1;
        return true;
    }
    else {
        for (size_t i = 0; i < this->focusable.size(); i++) {
            if (this->focusable[i] == focus) {
                adjustFocus(GetFocus(), focus);
                this->focused = i;
                return true;
            }
        }
    }
    return false;
}

IWindowPtr LayoutBase::FocusNext() {
    IWindowPtr oldFocus = GetFocus();
    if (++this->focused >= (int) this->focusable.size()) {
        this->focused = 0;
    }

    return adjustFocus(oldFocus, GetFocus());
}

IWindowPtr LayoutBase::FocusPrev() {
    IWindowPtr oldFocus = GetFocus();
    if (--this->focused < 0) {
        this->focused = (int) this->focusable.size() - 1;
    }

    return adjustFocus(oldFocus, GetFocus());
}

IWindowPtr LayoutBase::GetFocus() {
    if (this->focused >= 0 && this->focusable.size() > 0) {
        return this->focusable[this->focused];
    }

    return IWindowPtr();
}

bool LayoutBase::KeyPress(const std::string& key) {
    if (key == "KEY_LEFT" || key == "KEY_UP") {
        this->FocusPrev();
        return true;
    }
    else if (key == "KEY_RIGHT" || key == "KEY_DOWN") {
        this->FocusNext();
        return true;
    }

    return false;
}

ILayoutStack* LayoutBase::GetLayoutStack() {
    return this->layoutStack;
}

void LayoutBase::SetLayoutStack(ILayoutStack* stack) {
    this->layoutStack = stack;
}
