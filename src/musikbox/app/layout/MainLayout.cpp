#pragma once

#include "stdafx.h"
#include "MainLayout.h"
#include <cursespp/Screen.h>
#include <cursespp/IWindowMessage.h>

#define MESSAGE_TYPE_UPDATE 1001
#define UPDATE_INTERVAL_MS 1000

MainLayout::MainLayout(Transport& transport, LibraryPtr library)
: LayoutBase() {
    this->logs.reset(new LogWindow(this));
    this->output.reset(new OutputWindow(this));
    this->resources.reset(new ResourcesWindow(this));
    this->commands.reset(new CommandWindow(this, transport, library, *this->output));
    this->transport.reset(new TransportWindow(library, transport));

    this->AddWindow(this->commands);
    this->AddWindow(this->logs);
    this->AddWindow(this->output);
    this->AddWindow(this->resources);
    this->AddWindow(this->transport);

    Post(MESSAGE_TYPE_UPDATE, 0, 0, UPDATE_INTERVAL_MS);
}

MainLayout::~MainLayout() {

}

void MainLayout::Layout() {
    /* this layout */
    this->SetSize(Screen::GetWidth(), Screen::GetHeight());
    this->SetPosition(0, 0);
    this->SetFrameVisible(false);

    /* top left */
    this->transport->SetSize((Screen::GetWidth() / 2) - 2, 2);
    this->transport->SetPosition(1, 0);
    this->transport->Repaint();

    /* middle left */
    this->output->SetSize(Screen::GetWidth() / 2, Screen::GetHeight() - 3 - 2);
    this->output->SetPosition(0, 2);
    this->output->SetFocusOrder(1);

    /* bottom left */
    this->commands->SetSize(Screen::GetWidth() / 2, 3);
    this->commands->SetPosition(0, Screen::GetHeight() - 3);
    this->commands->SetFocusOrder(0);

    /* top right */
    this->logs->SetSize(Screen::GetWidth() / 2, Screen::GetHeight() - 3);
    this->logs->SetPosition(Screen::GetWidth() / 2, 0);
    this->logs->SetFocusOrder(2);

    /* bottom right */
    this->resources->SetSize(Screen::GetWidth() / 2, 3);
    this->resources->SetPosition(Screen::GetWidth() / 2, Screen::GetHeight() - 3);
}

void MainLayout::Show() {
    LayoutBase::Show();
    this->UpdateWindows();
}

void MainLayout::ProcessMessage(IWindowMessage &message) {
    if (message.MessageType() == MESSAGE_TYPE_UPDATE) {
        this->UpdateWindows();
        Post(MESSAGE_TYPE_UPDATE, 0, 0, UPDATE_INTERVAL_MS);
    }
}

void MainLayout::UpdateWindows() {
    this->logs->Update();
    this->resources->Update();
    this->transport->Update();
}