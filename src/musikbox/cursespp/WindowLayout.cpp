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
#include "WindowLayout.h"
#include "LayoutStack.h"

using namespace cursespp;

WindowLayout::WindowLayout(IWindowPtr window) {
    this->window = window;
    this->stack = 0;
}

WindowLayout::~WindowLayout() {
}

bool WindowLayout::AddWindow(IWindowPtr window) {
    throw std::runtime_error("AddWindow() not supported in LayoutStack. Use Push()");
}

bool WindowLayout::RemoveWindow(IWindowPtr window) {
    throw std::runtime_error("RemoveWindow() not supported in LayoutStack. Use Push()");
}

size_t WindowLayout::GetWindowCount() {
    throw std::runtime_error("GetWindowCount() not supported in LayoutStack.");
}

IWindowPtr WindowLayout::GetWindowAt(size_t position) {
    throw std::runtime_error("GetWindowAt() not supported in LayoutStack.");
}

void WindowLayout::Show() {
    this->window->Show();
}

void WindowLayout::Hide() {
    this->window->Hide();
}

bool WindowLayout::KeyPress(int64 ch) {
    if (ch == 27 && this->GetLayoutStack()) {
        this->GetLayoutStack()->Pop(shared_from_this());
    }
    return false;
}

IWindowPtr WindowLayout::FocusNext() {
    return this->window;
}

IWindowPtr WindowLayout::FocusPrev() {
    return this->window;
}

IWindowPtr WindowLayout::GetFocus() {
    return this->window;
}

void WindowLayout::BringToTop() {
    this->window->BringToTop();
}

void WindowLayout::SendToBottom() {
    this->window->SendToBottom();
}

ILayoutStack* WindowLayout::GetLayoutStack() {
    return this->stack;
}

void WindowLayout::SetLayoutStack(ILayoutStack* stack) {
    this->stack = stack;
}
