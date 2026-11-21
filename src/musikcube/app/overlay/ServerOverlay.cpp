//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

#include "ServerOverlay.h"

#include <app/util/Locale.h>

#include <cursespp/App.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/Screen.h>

using namespace musik;
using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace cursespp;

#define VERTICAL_PADDING 2
#define DEFAULT_HEIGHT 20
#define DEFAULT_WIDTH 45

#define RIGHT(x) (x->GetX() + x->GetWidth())
#define TEXT_WIDTH(x) ((int) u8cols(x->GetText()))

static void showInvalidDialog() {
    auto dialog = std::make_shared<DialogOverlay>();

    (*dialog)
        .SetTitle(_TSTR("settings_server_invalid_settings_title"))
        .SetMessage(_TSTR("settings_server_invalid_settings_message"))
        .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"));

    App::Overlays().Push(dialog);
}

static int getIntFromTextInput(TextInput* input, int defaultValue = -1) {
    const std::string value = input->GetText();
    if (value.size()) {
        try {
            return std::stoi(value);
        }
        catch (...) {
            /* swallow */
        }
    }
    return defaultValue;
}

ServerOverlay::ServerOverlay(const api::ServerConfiguration& configuration)
: OverlayBase()
{
    this->width = this->height = 0;
    this->InitViews();
    this->Load(configuration);
}

void ServerOverlay::InitViews() {
    /* title */
    this->titleLabel.reset(new TextLabel());
    this->titleLabel->SetText(_TSTR("settings_server_setup"), text::AlignCenter);
    this->titleLabel->SetBold(true);

    /* shortcuts */
    this->shortcuts.reset(new ShortcutsWindow());
    this->shortcuts->SetAlignment(text::AlignRight);
    this->shortcuts->AddShortcut("ESC", _TSTR("button_cancel"));
    this->shortcuts->AddShortcut("M-s", _TSTR("button_save"));

    this->shortcuts->SetChangedCallback([this](std::string key) {
        this->KeyPress(key);
    });

    /* web socket server */
    this->enableWssCb.reset(new Checkbox());
    this->enableWssCb->SetText(_TSTR("settings_server_enable_websockets"));

    this->wssPortLabel.reset(new TextLabel());
    this->wssPortLabel->SetText(_TSTR("settings_server_port"), text::AlignRight);
    this->wssPortInput.reset(new TextInput(TextInput::StyleLine));

    /* http server */
    this->enableHttpCb.reset(new Checkbox());
    this->enableHttpCb->SetText(_TSTR("settings_server_enable_http"));
    this->httpPortLabel.reset(new TextLabel());
    this->httpPortLabel->SetText(_TSTR("settings_server_port"), text::AlignRight);
    this->httpPortInput.reset(new TextInput(TextInput::StyleLine));

    /* ipv6 */
    this->ipv6Cb.reset(new Checkbox());
    this->ipv6Cb->SetText(_TSTR("settings_server_use_ipv6"));

    /* transcoder */
    this->enableSyncTransCb.reset(new Checkbox());
    this->enableSyncTransCb->SetText(_TSTR("settings_server_transcoder_synchronous"));

    this->transCacheLabel.reset(new TextLabel());
    this->transCacheLabel->SetText(_TSTR("settings_server_transcoder_cache_count"));
    this->transCacheInput.reset(new TextInput(TextInput::StyleLine));

    this->maxTransLabel = std::make_shared<TextLabel>();
    this->maxTransLabel->SetText(_TSTR("settings_server_maximum_transcoders"));
    this->maxTransInput = std::make_shared<TextInput>(TextInput::StyleLine);

    /* password */
    this->pwLabel.reset(new TextLabel());
    this->pwLabel->SetText(_TSTR("settings_server_password"), text::AlignRight);
    this->pwInput.reset(new TextInput(TextInput::StyleLine, IInput::InputPassword));

    /* style 'em */
    style(*this->titleLabel);
    style(*this->enableWssCb);
    style(*this->wssPortLabel);
    style(*this->wssPortInput);
    style(*this->enableHttpCb);
    style(*this->httpPortLabel);
    style(*this->httpPortInput);
    style(*this->ipv6Cb);
    style(*this->enableSyncTransCb);
    style(*this->transCacheLabel);
    style(*this->transCacheInput);
    style(*this->maxTransLabel);
    style(*this->maxTransInput);
    style(*this->pwLabel);
    style(*this->pwInput);

    /* add 'em */
    this->AddWindow(this->titleLabel);
    this->AddWindow(this->enableWssCb);
    this->AddWindow(this->wssPortLabel);
    this->AddWindow(this->wssPortInput);
    this->AddWindow(this->enableHttpCb);
    this->AddWindow(this->httpPortLabel);
    this->AddWindow(this->httpPortInput);
    this->AddWindow(this->ipv6Cb);
    this->AddWindow(this->enableSyncTransCb);
    this->AddWindow(this->transCacheLabel);
    this->AddWindow(this->transCacheInput);
    this->AddWindow(this->maxTransLabel);
    this->AddWindow(this->maxTransInput);
    this->AddWindow(this->pwLabel);
    this->AddWindow(this->pwInput);
    this->AddWindow(this->shortcuts);

    /* focus order */
    int order = 0;
    this->enableWssCb->SetFocusOrder(order++);
    this->wssPortInput->SetFocusOrder(order++);
    this->enableHttpCb->SetFocusOrder(order++);
    this->httpPortInput->SetFocusOrder(order++);
    this->ipv6Cb->SetFocusOrder(order++);
    this->enableSyncTransCb->SetFocusOrder(order++);
    this->transCacheInput->SetFocusOrder(order++);
    this->maxTransInput->SetFocusOrder(order++);
    this->pwInput->SetFocusOrder(order++);
}

void ServerOverlay::Layout() {
    this->RecalculateSize();
    this->MoveAndResize(this->x, this->y, this->width, this->height);

    const auto clientHeight = this->GetContentHeight();
    auto clientWidth = this->GetContentWidth();

    this->titleLabel->MoveAndResize(0, 0, clientWidth, 1);
    this->shortcuts->MoveAndResize(0, clientHeight - 1, clientWidth, 1);

    int x = 1;
    int y = 2;
    clientWidth -= 2;

    const int wssPortLabelWidth = TEXT_WIDTH(wssPortLabel);
    this->enableWssCb->MoveAndResize(x, y++, clientWidth, 1);
    this->wssPortLabel->MoveAndResize(x + 4, y, wssPortLabelWidth, 1);
    this->wssPortInput->MoveAndResize(x + 4 + wssPortLabelWidth + 1, y, 8, 1);
    y += 2;

    const int httpPortLabelWidth = TEXT_WIDTH(httpPortLabel);
    this->enableHttpCb->MoveAndResize(x, y++, clientWidth, 1);
    this->httpPortLabel->MoveAndResize(x + 4, y, httpPortLabelWidth, 1);
    this->httpPortInput->MoveAndResize(x + 4 + httpPortLabelWidth + 1, y, 8, 1);
    y += 2;

    this->ipv6Cb->MoveAndResize(x, y, clientWidth, 1);
    y += 2;

    const int transCacheLabelWidth = TEXT_WIDTH(transCacheLabel);
    this->enableSyncTransCb->MoveAndResize(x, y++, clientWidth, 1);
    this->transCacheLabel->MoveAndResize(x, y, transCacheLabelWidth, 1);
    this->transCacheInput->MoveAndResize(x + transCacheLabelWidth + 1, y, 5, 1);
    y += 1;

    const int maxTransLabelWidth = TEXT_WIDTH(maxTransLabel);
    this->maxTransLabel->MoveAndResize(x, y, maxTransLabelWidth, 1);
    this->maxTransInput->MoveAndResize(x + maxTransLabelWidth + 1, y, 3, 1);
    y += 2;

    const int pwLabelWidth = TEXT_WIDTH(pwLabel);
    this->pwLabel->MoveAndResize(x, y, pwLabelWidth, 1);
    this->pwInput->MoveAndResize(pwLabelWidth + 2, y, clientWidth - pwLabelWidth - 1, 1);
}

void ServerOverlay::ShowOverlay() {
    if (const auto configuration = api::ReadServerConfiguration()) {
        App::Overlays().Push(std::shared_ptr<ServerOverlay>(new ServerOverlay(*configuration)));
    }
}

bool ServerOverlay::Available() {
    return api::ReadServerConfiguration().has_value();
}

void ServerOverlay::Load(const api::ServerConfiguration& configuration) {
    this->enableWssCb->SetChecked(configuration.metadataEnabled);
    this->enableHttpCb->SetChecked(configuration.audioEnabled);
    this->enableSyncTransCb->SetChecked(configuration.synchronousTranscoding);

    this->wssPortInput->SetText(std::to_string(configuration.metadataPort));
    this->httpPortInput->SetText(std::to_string(configuration.audioPort));
    this->ipv6Cb->SetChecked(configuration.ipv6);
    this->transCacheInput->SetText(std::to_string(configuration.cacheCount));
    this->maxTransInput->SetText(std::to_string(configuration.activeTranscodeLimit));
    this->pwInput->SetText(configuration.password);
}

bool ServerOverlay::Save() {
    const int wssPort = getIntFromTextInput(this->wssPortInput.get(), MC_SERVER_DEFAULT_METADATA_PORT);
    const int httpPort = getIntFromTextInput(this->httpPortInput.get(), MC_SERVER_DEFAULT_AUDIO_PORT);
    const int cacheCount = getIntFromTextInput(this->transCacheInput.get(), MC_SERVER_DEFAULT_CACHE_COUNT);
    const int maxTransCount = getIntFromTextInput(this->maxTransInput.get(), MC_SERVER_DEFAULT_ACTIVE_TRANSCODE_LIMIT);

    if (wssPort <= 0 || wssPort > UINT16_MAX || httpPort <= 0 || httpPort > UINT16_MAX) {
        return false;
    }

    return api::ConfigureServer({
        this->enableWssCb->IsChecked(), this->enableHttpCb->IsChecked(), this->ipv6Cb->IsChecked(),
        this->enableSyncTransCb->IsChecked(), static_cast<uint16_t>(wssPort), static_cast<uint16_t>(httpPort),
        cacheCount, maxTransCount, this->pwInput->GetText()
    }) == MC_OK;
}

bool ServerOverlay::KeyPress(const std::string& key) {
    if (key == "^[" || key == "ESC") { /* esc closes */
        this->Dismiss();
        return true;
    }
    else if (key == "M-s") {
        if (Save()) {
            this->Dismiss();
        }
        else {
            showInvalidDialog();
        }
        return true;
    }

    return OverlayBase::KeyPress(key);
}

void ServerOverlay::RecalculateSize() {
    this->width = _DIMEN("server_overlay_width", DEFAULT_WIDTH);
    this->height = std::max(0, std::min(Screen::GetHeight() - 2, DEFAULT_HEIGHT));
    this->width = std::max(0, std::min(Screen::GetWidth(), this->width));

    this->y = VERTICAL_PADDING;
    this->x = (Screen::GetWidth() / 2) - (this->width / 2);
}
