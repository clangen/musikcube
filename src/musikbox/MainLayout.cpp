#pragma once

#include "stdafx.h"
#include "MainLayout.h"
#include "Screen.h"

MainLayout::MainLayout(Transport& transport, LibraryPtr library)
: LayoutBase() 
{
    this->SetSize(Screen::GetWidth(), Screen::GetHeight());
    this->SetPosition(0, 0);
    this->SetFrameVisible(false);
    this->Show();

    this->logs.reset(new LogWindow(this));
    this->logs->SetFocusOrder(2);

    this->output.reset(new OutputWindow(this));
    this->output->SetFocusOrder(1);
    
    this->resources.reset(new ResourcesWindow(this));

    this->commands.reset(new CommandWindow(this, transport, library, *this->output));
    this->commands->SetFocusOrder(0);

    this->transport.reset(new TransportWindow(this, transport));

    this->AddWindow(this->commands);
    this->AddWindow(this->logs);
    this->AddWindow(this->output);
    this->AddWindow(this->resources);
    this->AddWindow(this->transport);

    this->Layout();
}

MainLayout::~MainLayout() {

}

void MainLayout::Layout() {
    /* top left */
    this->transport->SetSize(Screen::GetWidth() / 2, 4);
    this->transport->SetPosition(0, 0);
    this->transport->Show();
    this->transport->Repaint();

    /* middle left */
    this->output->SetSize(Screen::GetWidth() / 2, Screen::GetHeight() - 3 - 4);
    this->output->SetPosition(0, 4);
    this->output->Show();
    this->output->Repaint();

    /* bottom left */
    this->commands->SetSize(Screen::GetWidth() / 2, 3);
    this->commands->SetPosition(0, Screen::GetHeight() - 3);
    this->commands->Show();

    /* top right */
    this->logs->SetSize(Screen::GetWidth() / 2, Screen::GetHeight() - 3);
    this->logs->SetPosition(Screen::GetWidth() / 2, 0);
    this->logs->Show();

    /* bottom right */
    this->resources->SetSize(Screen::GetWidth() / 2, 3);
    this->resources->SetPosition(Screen::GetWidth() / 2, Screen::GetHeight() - 3);
    this->resources->Show();
    this->resources->Repaint();
}

void MainLayout::OnIdle() {
    this->logs->Update();
    this->transport->Repaint();
    this->resources->Repaint();
}