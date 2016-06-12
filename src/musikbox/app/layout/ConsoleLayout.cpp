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

#include "stdafx.h"
#include "ConsoleLayout.h"
#include <cursespp/Screen.h>
#include <cursespp/IMessage.h>

#define MESSAGE_TYPE_UPDATE 1001
#define UPDATE_INTERVAL_MS 1000

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::box;
using namespace cursespp;

ConsoleLayout::ConsoleLayout(ITransport& transport, LibraryPtr library)
: LayoutBase() {
    this->logs.reset(new LogWindow(this));
    this->output.reset(new OutputWindow(this));
    this->resources.reset(new ResourcesWindow(this));

    this->commands.reset(new CommandWindow(
        this,
        transport,
        library,
        *this->output,
        *this->logs));

    this->AddWindow(this->commands);
    this->AddWindow(this->logs);
    this->AddWindow(this->output);
    this->AddWindow(this->resources);

    this->PostMessage(MESSAGE_TYPE_UPDATE, 0, 0, UPDATE_INTERVAL_MS);
}

ConsoleLayout::~ConsoleLayout() {

}

void ConsoleLayout::Layout() {
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

void ConsoleLayout::Show() {
    LayoutBase::Show();
    this->UpdateWindows();
}

void ConsoleLayout::ProcessMessage(IMessage &message) {
    if (message.Type() == MESSAGE_TYPE_UPDATE) {
        this->UpdateWindows();
        this->PostMessage(MESSAGE_TYPE_UPDATE, 0, 0, UPDATE_INTERVAL_MS);
    }
}

void ConsoleLayout::UpdateWindows() {
    this->logs->Update();
    this->resources->Update();
}
