#include "stdafx.h"
#include "LayoutBase.h"

LayoutBase::LayoutBase(IWindow* parent) 
: Window(parent) {
    this->SetFrameVisible(false);
}

LayoutBase::~LayoutBase() {

}

void LayoutBase::Create() {
    Window::Create();

    for (size_t i = 0; i < this->children.size(); i++) {
        this->children.at(i)->Create();
    }
}

void LayoutBase::Destroy() {
    for (size_t i = 0; i < this->children.size(); i++) {
        this->children.at(i)->Destroy();
    }

    Window::Destroy();
}


bool LayoutBase::AddWindow(IWindowPtr window) {
    std::vector<IWindowPtr>::iterator it = this->children.begin();
    for (; it != this->children.end(); it++) {
        if (*it == window) {
            return true;
        }
    }

    window->SetParent(this);
    this->children.push_back(window);

    return true;
}

bool LayoutBase::RemoveWindow(IWindowPtr window) {
    std::vector<IWindowPtr>::iterator it = this->children.begin();
    for ( ; it != this->children.end(); it++) {
        if (*it == window) {
            this->children.erase(it);
            return true;
        }
    }

    return false;
}

size_t LayoutBase::GetWindowCount() {
    return this->children.size();
}

IWindowPtr LayoutBase::GetWindowAt(size_t position) {
    return this->children.at(position);
}