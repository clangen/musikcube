#pragma once

#include "stdafx.h"
#include "MainLayout.h"
#include "Screen.h"

MainLayout::MainLayout(Transport& transport, LibraryPtr library)
: LayoutBase() {
    this->logs.reset(new LogWindow(this));
    this->output.reset(new OutputWindow(this));
    this->resources.reset(new ResourcesWindow(this));
    this->commands.reset(new CommandWindow(this, transport, library, *this->output));
    this->transport.reset(new TransportWindow(this, transport));

    this->AddWindow(this->commands);
    this->AddWindow(this->logs);
    this->AddWindow(this->output);
    this->AddWindow(this->resources);
    this->AddWindow(this->transport);
}

MainLayout::~MainLayout() {

}

void MainLayout::Layout() {
    /* this layout */
    this->SetSize(Screen::GetWidth(), Screen::GetHeight());
    this->SetPosition(0, 0);
    this->SetFrameVisible(false);

    /* top left */
    this->transport->SetSize(Screen::GetWidth() / 2, 4);
    this->transport->SetPosition(0, 0);
    this->transport->Repaint();

    /* middle left */
    this->output->SetSize(Screen::GetWidth() / 2, Screen::GetHeight() - 3 - 4);
    this->output->SetPosition(0, 4);
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

void MainLayout::OnIdle() {
    this->UpdateWindows();
}

void MainLayout::UpdateWindows() {
    this->logs->Update();
    this->transport->Repaint();
    this->resources->Repaint();
}