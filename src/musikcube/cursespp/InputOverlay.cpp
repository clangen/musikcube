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
#include <cursespp/App.h>
#include <cursespp/InputOverlay.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/Colors.h>
#include <cursespp/Screen.h>
#include <cursespp/Text.h>
#include <core/i18n/Locale.h>

using namespace cursespp;

#define VERTICAL_PADDING 2
#define DEFAULT_WIDTH 26
#define MAX_HEIGHT 12

#define DISMISS() \
    OverlayStack* overlays = this->GetOverlayStack(); \
    if (overlays) { \
        overlays->Remove(this); \
    }

InputOverlay::InputOverlay() {
    this->SetFrameVisible(true);
    this->SetFrameColor(Color::OverlayFrame);
    this->SetContentColor(Color::OverlayContent);

    this->allowEmptyValue = false;
    this->width = this->height = this->setWidth = 0;

    this->textInput.reset(new TextInput());
    this->textInput->SetFocusOrder(0);
    this->textInput->EnterPressed.connect(this, &InputOverlay::OnInputEnterPressed);
    this->textInput->TextChanged.connect(this, &InputOverlay::OnInputKeyPress);
    this->AddWindow(this->textInput);
    style(*this->textInput);
}

InputOverlay::~InputOverlay() {
}

void InputOverlay::Layout() {
    this->RecalculateSize();

    if (this->width > 0 && this->height > 0) {
        this->MoveAndResize(
            this->x,
            this->y,
            this->width,
            this->height);

        this->textInput->MoveAndResize(
            1, /* one pixel padding L and R */
            2, /* below the title, plus an extra space */
            this->GetContentWidth() - 2,
            3); /* top and bottom border plus text */

        this->Redraw();
    }
}

InputOverlay& InputOverlay::SetTitle(const std::string& title) {
    this->title = title;
    this->RecalculateSize();
    this->Layout();
    this->Invalidate();
    return *this;
}

InputOverlay& InputOverlay::SetText(const std::string& text) {
    this->textInput->SetText(text);
    return *this;
}

InputOverlay& InputOverlay::SetAllowEmptyValue(bool allowEmptyValue) {
    this->allowEmptyValue = allowEmptyValue;
    return *this;
}

InputOverlay& InputOverlay::SetWidth(int width) {
    this->setWidth = width;

    if (this->IsVisible()) {
        this->Layout();
    }

    return *this;
}

void InputOverlay::OnInputKeyPress(TextInput* input, std::string key) {
    /* raw input mode will not allow the ESC key to propagate.
    we can catch it here, and pass it through to the regular key
    handler to close the dialog */
    if (input->GetInputMode() == IInput::InputRaw && key == "^[") {
        this->KeyPress(key);
    }
}

bool InputOverlay::KeyPress(const std::string& key) {
    if (key == "^[") { /* esc closes */
        this->Dismiss();
        return true;
    }

    return LayoutBase::KeyPress(key);
}

void InputOverlay::OnInputEnterPressed(TextInput* input) {
    if (input->GetText().size() || this->allowEmptyValue) {
        if (validator && !validator->IsValid(input->GetText())) {
            std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

            (*dialog)
                .SetTitle(_TSTR("validator_dialog_title"))
                .SetMessage(validator->ErrorMessage())
                .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"));

            App::Overlays().Push(dialog);
        }
        else {
            if (inputAcceptedCallback) {
                inputAcceptedCallback(input->GetText());
            }

            this->Dismiss();
        }
    }
}

InputOverlay& InputOverlay::SetInputMode(IInput::InputMode mode) {
    this->textInput->SetInputMode(mode);
    return *this;
}

InputOverlay& InputOverlay::SetValidator(std::shared_ptr<IValidator> validator) {
    this->validator = validator;
    return *this;
}

InputOverlay& InputOverlay::SetInputAcceptedCallback(InputAcceptedCallback cb) {
    this->inputAcceptedCallback = cb;
    return *this;
}

void InputOverlay::OnVisibilityChanged(bool visible) {
    if (visible) {
        this->Redraw();
    }
}

void InputOverlay::RecalculateSize() {
    this->width = this->setWidth > 0 ? this->setWidth : DEFAULT_WIDTH;
    this->height = 1 + 1 + 1 + 3 + 1; /* top frame + text + space + text input + bottom frame */

    /* constrain to app bounds */
    this->height = std::max(0, std::min(Screen::GetHeight() - 4, this->height));
    this->width = std::max(0, std::min(Screen::GetWidth(), this->width));

    this->y = VERTICAL_PADDING;
    this->x = (Screen::GetWidth() / 2) - (this->width / 2);
}

void InputOverlay::Redraw() {
    if (!this->IsVisible() || this->width <= 0 || this->height <= 0) {
        return;
    }

    WINDOW* c = this->GetContent();

    if (this->title.size()) {
        wmove(c, 0, 1);
        wattron(c, A_BOLD);
        checked_wprintw(c, text::Align(this->title, text::AlignCenter, this->width - 4).c_str());
        wattroff(c, A_BOLD);
    }
}
