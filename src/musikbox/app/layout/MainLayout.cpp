#include "stdafx.h"
#include "MainLayout.h"
#include <cursespp/Screen.h>
#include <cursespp/IMessage.h>

#define MESSAGE_TYPE_UPDATE 1001
#define UPDATE_INTERVAL_MS 1000

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::box;
using namespace cursespp;

MainLayout::MainLayout(Transport& transport, LibraryPtr library)
: LayoutBase() {
    this->logs.reset(new LogWindow(this));
    this->output.reset(new OutputWindow(this));
    this->resources.reset(new ResourcesWindow(this));
    this->commands.reset(new CommandWindow(this, transport, library, *this->output));

    this->AddWindow(this->commands);
    this->AddWindow(this->logs);
    this->AddWindow(this->output);
    this->AddWindow(this->resources);

    this->PostMessage(MESSAGE_TYPE_UPDATE, 0, 0, UPDATE_INTERVAL_MS);
}

MainLayout::~MainLayout() {

}

void MainLayout::Layout() {
    /* this layout */
    this->MoveAndResize(
        0,
        0,
        Screen::GetWidth(),
        Screen::GetHeight());

    this->SetFrameVisible(false);

    /* top left */
    this->output->MoveAndResize(
        0,
        0,
        Screen::GetWidth() / 2,
        Screen::GetHeight() - 3);

    this->output->SetFocusOrder(1);

    /* bottom left */
    this->commands->MoveAndResize(
        0,
        Screen::GetHeight() - 3,
        Screen::GetWidth() / 2,
        3);

    this->commands->SetFocusOrder(0);

    /* top right */
    this->logs->MoveAndResize(
        Screen::GetWidth() / 2,
        0,
        Screen::GetWidth() / 2,
        Screen::GetHeight() - 3);

    this->logs->SetFocusOrder(2);

    /* bottom right */
    this->resources->MoveAndResize(
        Screen::GetWidth() / 2,
        Screen::GetHeight() - 3,
        Screen::GetWidth() / 2,
        3);
}

void MainLayout::Show() {
    LayoutBase::Show();
    this->UpdateWindows();
}

void MainLayout::ProcessMessage(IMessage &message) {
    if (message.Type() == MESSAGE_TYPE_UPDATE) {
        this->UpdateWindows();
        this->PostMessage(MESSAGE_TYPE_UPDATE, 0, 0, UPDATE_INTERVAL_MS);
    }
}

void MainLayout::UpdateWindows() {
    this->logs->Update();
    this->resources->Update();
}
