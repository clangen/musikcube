#pragma once

#include "stdafx.h"
#include "MainLayout.h"
#include "Screen.h"

/* most top-level layouts are going to want this functionality. it
should probably live someplace shared. */
static inline IWindow* adjustFocus(IWindow* oldFocus, IWindow* newFocus) {
    if (oldFocus) {
        oldFocus->SetFrameColor(BOX_COLOR_WHITE_ON_BLACK);
    }

    if (newFocus) {
        newFocus->SetFrameColor(BOX_COLOR_RED_ON_BLACK);
    }

    return newFocus;
}

MainLayout::MainLayout(Transport& transport, LibraryPtr library) {
    this->SetSize(Screen::GetWidth(), Screen::GetHeight());
    this->SetPosition(0, 0);
    this->SetFrameVisible(false);
    this->Create();

    this->logs.reset(new LogWindow(this));
    this->output.reset(new OutputWindow(this));
    this->resources.reset(new ResourcesWindow(this));
    this->commands.reset(new CommandWindow(this, transport, library, *this->output));
    this->transport.reset(new TransportWindow(this, transport));

    this->focusOrder.push_back(commands.get());
    this->focusOrder.push_back(output.get());
    this->focusOrder.push_back(logs.get());
    this->focusIndex = 0;

    this->Layout();

    adjustFocus(NULL, GetFocus());
}

MainLayout::~MainLayout() {

}

IWindow* MainLayout::FocusNext() {
    IWindow* oldFocus = GetFocus();
    if (++focusIndex >= (int) focusOrder.size()) {
        focusIndex = 0;
    }

    return adjustFocus(oldFocus, GetFocus());
}

IWindow* MainLayout::FocusPrev() {
    IWindow* oldFocus = GetFocus();
    if (--focusIndex <= 0) {
        focusIndex = (int) focusOrder.size() - 1;
    }

    return adjustFocus(oldFocus, GetFocus());
}

IWindow* MainLayout::GetFocus() {
    return focusOrder[focusIndex];
}

void MainLayout::Layout() {
    /* top left */
    this->transport->SetSize(Screen::GetWidth() / 2, 4);
    this->transport->SetPosition(0, 0);
    this->transport->Create();
    this->transport->Repaint();

    /* middle left */
    this->output->SetSize(Screen::GetWidth() / 2, Screen::GetHeight() - 3 - 4);
    this->output->SetPosition(0, 4);
    this->output->Create();
    this->output->Repaint();

    /* bottom left */
    this->commands->SetSize(Screen::GetWidth() / 2, 3);
    this->commands->SetPosition(0, Screen::GetHeight() - 3);
    this->commands->Create();

    /* top right */
    this->logs->SetSize(Screen::GetWidth() / 2, Screen::GetHeight() - 3);
    this->logs->SetPosition(Screen::GetWidth() / 2, 0);
    this->logs->Create();

    /* bottom right */
    this->resources->SetSize(Screen::GetWidth() / 2, 3);
    this->resources->SetPosition(Screen::GetWidth() / 2, Screen::GetHeight() - 3);
    this->resources->Create();
    this->resources->Repaint();
}

void MainLayout::OnIdle() {
    this->logs->Update();
    this->transport->Repaint();
    this->resources->Repaint();
}