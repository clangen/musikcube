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
