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

bool LayoutStack::KeyPress(int64 ch) {
    if (!this->layouts.size()) {
        return false;
    }

    return this->layouts.front()->KeyPress(ch);
}

IWindowPtr LayoutStack::FocusNext() {
    if (!this->layouts.size()) {
        return IWindowPtr();
    }

    return this->layouts.front()->FocusNext();
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
