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
#include <climits>
#include <algorithm>
#include <cursespp/LayoutBase.h>
#include <cursespp/Colors.h>

using namespace cursespp;

static const int NO_FOCUS = -2;

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

LayoutBase::LayoutBase(IWindow* parent)
: Window(parent)
, focused(0)
, focusMode(FocusModeCircular) {
    this->SetFrameVisible(false);
}

LayoutBase::~LayoutBase() {
    for (IWindowPtr window : this->children) {
        window->SetParent(nullptr);
    }
}

void LayoutBase::Focus() {
    Window::Focus();
    this->FocusFirst();
}

IWindowPtr LayoutBase::EnsureValidFocus() {
    auto newFocus = this->GetFocus();
    if (newFocus && this->IsVisible()) {
        newFocus->Focus();
    }
    return newFocus;
}

IWindowPtr LayoutBase::EnsureValidFocusFromNext() {
    auto newFocus = this->GetFocus();
    if (newFocus && this->IsVisible()) {
        LayoutBase* layout = dynamic_cast<LayoutBase*>(newFocus.get());
        if (layout) {
            layout->FocusFirst();
        }
        else {
            newFocus->Focus();
        }
    }
    return newFocus;
}

IWindowPtr LayoutBase::EnsureValidFocusFromPrev() {
    auto newFocus = this->GetFocus();
    if (newFocus && this->IsVisible()) {
        LayoutBase* layout = dynamic_cast<LayoutBase*>(newFocus.get());
        if (layout) {
            layout->FocusLast();
        }
        else {
            newFocus->Focus();
        }
    }
    return newFocus;
}

void LayoutBase::OnVisibilityChanged(bool visible) {
    if (visible) {
        for (IWindowPtr window : this->children) {
            window->OnParentVisibilityChanged(true);
        }

        this->IndexFocusables();
        this->SortFocusables();
    }
    else {
        for (IWindowPtr window : this->children) {
            window->OnParentVisibilityChanged(false);
        }
    }
}

void LayoutBase::Show() {
    Window::Show();
    this->Layout();
}

void LayoutBase::Hide() {
    Window::Hide();
}

void LayoutBase::Layout() {
    if (this->IsVisible() && !this->CheckForBoundsError()) {
        this->OnLayout();
    }
}

void LayoutBase::OnLayout() {
    /* most layouts will want to perform layout logic here... */
}

void LayoutBase::OnParentVisibilityChanged(bool visible) {
    Window::OnParentVisibilityChanged(visible);

    for (IWindowPtr window : this->children) {
        window->OnParentVisibilityChanged(visible);
    }
}

void LayoutBase::OnChildVisibilityChanged(bool visible, IWindow* child) {
    Window::OnChildVisibilityChanged(visible, child);
    this->IndexFocusables();
}

void LayoutBase::BringToTop() {
    Window::BringToTop();

    for (IWindowPtr window : this->children) {
        window->BringToTop();
    }

    this->Invalidate();
}

void LayoutBase::SendToBottom() {
    for (IWindowPtr window : this->children) {
        window->SendToBottom();
    }

    Window::SendToBottom();
}

void LayoutBase::Invalidate() {
    /* repaint bottom up. start with ourselves, then our children,
    recursively. */

    Window::Invalidate();

    for (IWindowPtr window : this->children) {
        window->Invalidate();
    }
}

bool LayoutBase::AddWindow(IWindowPtr window) {
    if (!window) {
        throw std::runtime_error("window cannot be null!");
    }

    if (find(this->children, window) >= 0) {
        return true;
    }

    window->SetParent(this);

    this->children.push_back(window);
    AddFocusable(window);
    window->Show();

    return true;
}

bool LayoutBase::RemoveWindow(IWindowPtr window) {
    this->RemoveFocusable(window);

    std::vector<IWindowPtr>::iterator it = this->children.begin();
    for ( ; it != this->children.end(); it++) {
        if (*it == window) {
            (*it)->Hide();
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
    for (IWindowPtr window : this->children) {
        if (window->IsVisible()) {
            AddFocusable(window);
        }
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
        this->focused = NO_FOCUS;
        this->EnsureValidFocus();
        return true;
    }
    else {
        for (size_t i = 0; i < this->focusable.size(); i++) {
            if (this->focusable[i] == focus) {
                /* if we're focused, we need to ensure our parent sets its correct
                focus index to us! and so on up to the root view. note that this
                needs to be called before updating our internal focus; recursively,
                parents need to have their focus index set before children */
                auto asLayout = dynamic_cast<ILayout*>(this->GetParent());
                if (asLayout) {
                    asLayout->SetFocus(shared_from_this());
                }
                this->focused = i;
                this->EnsureValidFocus();
                return true;
            }
        }
    }
    return false;
}

IWindowPtr LayoutBase::FocusNext() {
    sigslot::signal1<FocusDirection>* notify = nullptr;

    /* GetFocus() will return the focused Window recursively. once we know
    what it is, see if it can focus next. if it cannot, see if its parent
    can, and so on, until we reach ourselves. reaching ourself is the base
    case, and we drop through to the actual focus logic below.

    NOTE: IF YOU UPDATE THIS LOGIC, ALSO UPDATE THE CORRESPONDING CODE IN
    LayoutBase::FocusPrev() */
    auto currFocus = this->GetFocus().get();
    if (currFocus) {
        do {
            auto asLayout = dynamic_cast<LayoutBase*>(currFocus);
            if (asLayout) {
                auto nextFocus = asLayout->FocusNext();
                if (nextFocus) {
                    if (notify) {
                        (*notify)(FocusForward);
                    }
                    return nextFocus;
                }
            }

            Window* parent = (Window*) currFocus->GetParent();
            if (parent == this) { /* base case / stop condition. this should always be hit */
                break;
            }
            currFocus = parent;
        } while (true);
    }

    /*** actual focus logic starts here ***/

    if (this->focused == NO_FOCUS && this->focusMode == FocusModeTerminating) {
        /* nothing. we're already terminated. */
        notify = &FocusTerminated;
    }
    else {
        ++this->focused;
        if (this->focused >= (int) this->focusable.size()) {
            if (this->focusMode == FocusModeCircular) {
                this->focused = 0;
                notify = &FocusWrapped;
            }
            else {
                this->focused = NO_FOCUS;
                notify = &FocusTerminated;
            }
        }
    }

    this->EnsureValidFocusFromNext();

    if (notify) {
        (*notify)(FocusForward);
    }

    return this->GetFocus();
}

IWindowPtr LayoutBase::FocusPrev() {
    sigslot::signal1<FocusDirection>* notify = nullptr;

    /* GetFocus() will return the focused Window recursively. once we know
    what it is, see if it can focus next. if it cannot, see if its parent
    can, and so on, until we reach ourselves. reaching ourself is the base
    case, and we drop through to the actual focus logic below.

    NOTE: IF YOU UPDATE THIS LOGIC, ALSO UPDATE THE CORRESPONDING CODE IN
    LayoutBase::FocusNext() */
    auto currFocus = this->GetFocus().get();
    if (currFocus) {
        do {
            auto asLayout = dynamic_cast<LayoutBase*>(currFocus);
            if (asLayout) {
                auto prevFocus = asLayout->FocusPrev();
                if (prevFocus) {
                    if (notify) {
                        (*notify)(FocusForward);
                    }
                    return prevFocus;
                }
            }

            Window* parent = (Window*) currFocus->GetParent();
            if (parent == this) { /* base case/stop condition. this should always be hit */
                break;
            }
            currFocus = parent;
        } while (true);
    }

    /*** actual focus logic starts here ***/

    --this->focused;
    if (this->focused < 0) {
        if (this->focusMode == FocusModeCircular) {
            this->focused = (int) this->focusable.size() - 1;
            notify = &FocusWrapped;
        }
        else {
            this->focused = NO_FOCUS;
            notify = &FocusTerminated;
        }
    }

    this->EnsureValidFocusFromPrev();

    if (notify) {
        (*notify)(FocusBackward);
    }

    return GetFocus();
}

IWindowPtr LayoutBase::FocusFirst() {
    this->focused = 0;
    return this->EnsureValidFocus();
}

IWindowPtr LayoutBase::FocusLast() {
    this->focused = (int) this->focusable.size() - 1;
    return this->EnsureValidFocus();
}

IWindowPtr LayoutBase::GetFocus() {
    if (this->focused >= 0 && (int) this->focusable.size() > this->focused) {
        auto view = this->focusable[this->focused];
        if (view->IsVisible()) {
            /* see if the currently focused view is a layout. if it is, see if it
            has something focused. if it does, return that. if it doesn't, just
            return the view itself. this is confusing, but allows nested circular
            layouts to function automatically */
            LayoutBase* asLayoutBase = dynamic_cast<LayoutBase*>(view.get());
            if (asLayoutBase) {
                auto focused = asLayoutBase->GetFocus();
                if (focused) {
                    return focused;
                }
            }
            return view;
        }
    }

    return IWindowPtr();
}

int LayoutBase::GetFocusIndex() {
    return this->focused;
}

void LayoutBase::SetFocusIndex(int index, bool applyFocus) {
    if (!this->focusable.size()) {
        this->IndexFocusables();
    }
    this->focused = index;
    if (applyFocus) {
        this->EnsureValidFocus();
    }
}

int LayoutBase::GetFocusableCount() {
    return (int) this->focusable.size();
}

IWindowPtr LayoutBase::GetFocusableAt(int index) {
    return this->focusable.at(index);
}

ILayout::FocusMode LayoutBase::GetFocusMode() const {
    return this->focusMode;
}

void LayoutBase::SetFocusMode(FocusMode mode) {
    this->focusMode = mode;
}

bool LayoutBase::KeyPress(const std::string& key) {
    auto& keys = NavigationKeys();
    if (keys.Left(key) || keys.Up(key)) {
        this->FocusPrev();
        return true;
    }
    else if (keys.Right(key) || keys.Down(key)) {
        this->FocusNext();
        return true;
    }
    return false;
}

bool LayoutBase::MouseEvent(const IMouseHandler::Event& mouseEvent) {
    for (auto window : this->children) {
        auto x = window->GetX();
        auto y = window->GetY();
        auto cx = window->GetWidth();
        auto cy = window->GetHeight();
        if (mouseEvent.x >= x && mouseEvent.x < x + cx &&
            mouseEvent.y >= y && mouseEvent.y < y + cy)
        {
            auto relative = IMouseHandler::Event(mouseEvent, window.get());
            if (window->MouseEvent(relative)) {
                return true;
            }
        }
    }
    return false;
}