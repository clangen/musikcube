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

#include "ServerOverlay.h"

#include <core/plugin/PluginFactory.h>
#include <core/i18n/Locale.h>

#include <app/util/PreferenceKeys.h>

#include <cursespp/App.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/Screen.h>

using namespace musik;
using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::box;
using namespace cursespp;

static const std::string WEBSOCKET_PLUGIN_GUID = "9fc897a3-dfd5-4524-a0fc-b02f46aea4a9";
static const char* KEY_METADATA_SERVER_ENABLED = "websocket_server_enabled";
static const char* KEY_METADATA_SERVER_PORT = "websocket_server_port";
static const char* KEY_AUDIO_SERVER_ENABLED = "http_server_enabled";
static const char* KEY_AUDIO_SERVER_PORT = "http_server_port";
static const char* KEY_TRANSCODER_CACHE_COUNT = "transcoder_cache_count";
static const char* KEY_TRANSCODER_SYNCHRONOUS = "transcoder_synchronous";
static const char* KEY_PASSWORD = "password";

#define VERTICAL_PADDING 1
#define DEFAULT_HEIGHT 18
#define DEFAULT_WIDTH 45
#define PORT_INPUT_WIDTH 10
#define PORT_INPUT_HEIGHT 3

#define STYLE_OVERLAY_LABEL(x) \
    x->SetContentColor(CURSESPP_OVERLAY_CONTENT);

#define STYLE_OVERLAY_CHECKBOX(x) \
    x->SetContentColor(CURSESPP_OVERLAY_CONTENT); \
    x->SetFocusedContentColor(CURSESPP_OVERLAY_TEXT_FOCUSED);

#define STYLE_OVERLAY_INPUT(x) \
    x->SetFrameColor(CURSESPP_OVERLAY_FRAME); \
    x->SetContentColor(CURSESPP_OVERLAY_CONTENT); \
    x->SetFocusedFrameColor(CURSESPP_OVERLAY_INPUT_FRAME); \
    x->SetFocusedContentColor(CURSESPP_OVERLAY_CONTENT);

#define RIGHT(x) (x->GetX() + x->GetWidth())
#define TEXT_WIDTH(x) ((int) u8cols(x->GetText()))

using Callback = ServerOverlay::Callback;
using Prefs = ServerOverlay::Prefs;

static std::string settingIntToString(Prefs prefs, const std::string& key, int defaultValue) {
    int resolved = prefs->GetInt(key, defaultValue);
    try {
        return std::to_string(resolved);
    }
    catch (...) {
        return std::to_string(defaultValue);
    }
}

static void showInvalidDialog(Callback cb = Callback()) {
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    (*dialog)
        .SetTitle(_TSTR("settings_server_invalid_settings_title"))
        .SetMessage(_TSTR("settings_server_invalid_settings_message"))
        .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"), [cb](std::string key) {
        if (cb) {
            cb();
        }
    });

    App::Overlays().Push(dialog);
}

static int getIntFromTextInput(TextInput* input) {
    const std::string value = input->GetText();
    if (value.size()) {
        try {
            return std::stoi(value);
        }
        catch (...) {
            /* swallow */
        }
    }
    return -1;
}

ServerOverlay::ServerOverlay(Callback callback, Plugin plugin)
: OverlayBase() {
    this->callback = callback;
    this->plugin = plugin;
    this->prefs = Preferences::ForPlugin(plugin->Name());
    this->width = this->height = 0;

    this->SetFrameVisible(true);
    this->SetFrameColor(CURSESPP_OVERLAY_FRAME);
    this->SetContentColor(CURSESPP_OVERLAY_CONTENT);

    this->InitViews();
    this->Load();
}

void ServerOverlay::InitViews() {
    /* title */
    this->titleLabel.reset(new TextLabel());
    this->titleLabel->SetText(_TSTR("settings_server_setup"), text::AlignCenter);

    /* shortcuts */
    this->shortcuts.reset(new ShortcutsWindow());
    this->shortcuts->SetAlignment(text::AlignRight);
    this->shortcuts->AddShortcut("ESC", _TSTR("button_cancel"));
    this->shortcuts->AddShortcut("M-s", _TSTR("button_save"));

    /* web socket server */
    this->enableWssCb.reset(new Checkbox());
    this->enableWssCb->SetText(_TSTR("settings_server_enable_websockets"));

    this->wssPortLabel.reset(new TextLabel());
    this->wssPortLabel->SetText(_TSTR("settings_server_metadata_port"), text::AlignRight);
    this->wssPortInput.reset(new TextInput());

    /* http server */
    this->enableHttpCb.reset(new Checkbox());
    this->enableHttpCb->SetText(_TSTR("settings_server_enable_http"));
    this->httpPortLabel.reset(new TextLabel());
    this->httpPortLabel->SetText(_TSTR("settings_server_audio_port"), text::AlignRight);
    this->httpPortInput.reset(new TextInput());

    /* transcoder */
    this->enableSyncTransCb.reset(new Checkbox());
    this->enableSyncTransCb->SetText(_TSTR("settings_server_transcoder_synchronous"));

    /* password */
    this->pwLabel.reset(new TextLabel());
    this->pwLabel->SetText(_TSTR("settings_server_password"), text::AlignRight);
    this->pwInput.reset(new TextInput(IInput::InputPassword));

    /* style 'em */
    STYLE_OVERLAY_LABEL(this->titleLabel);
    STYLE_OVERLAY_CHECKBOX(this->enableWssCb);
    STYLE_OVERLAY_LABEL(this->wssPortLabel);
    STYLE_OVERLAY_INPUT(this->wssPortInput);
    STYLE_OVERLAY_CHECKBOX(this->enableHttpCb);
    STYLE_OVERLAY_LABEL(this->httpPortLabel);
    STYLE_OVERLAY_INPUT(this->httpPortInput);
    STYLE_OVERLAY_CHECKBOX(this->enableSyncTransCb);
    STYLE_OVERLAY_LABEL(this->pwLabel);
    STYLE_OVERLAY_INPUT(this->pwInput);

    /* add 'em */
    this->AddWindow(this->titleLabel);
    this->AddWindow(this->enableWssCb);
    this->AddWindow(this->enableHttpCb);
    this->AddWindow(this->enableSyncTransCb);
    this->AddWindow(this->wssPortLabel);
    this->AddWindow(this->wssPortInput);
    this->AddWindow(this->httpPortLabel);
    this->AddWindow(this->httpPortInput);
    this->AddWindow(this->pwLabel);
    this->AddWindow(this->pwInput);
    this->AddWindow(this->shortcuts);

    /* focus order */
    int order = 0;
    this->enableWssCb->SetFocusOrder(order++);
    this->enableHttpCb->SetFocusOrder(order++);
    this->enableSyncTransCb->SetFocusOrder(order++);
    this->wssPortInput->SetFocusOrder(order++);
    this->httpPortInput->SetFocusOrder(order++);
    this->pwInput->SetFocusOrder(order++);
}

void ServerOverlay::Layout() {
    this->RecalculateSize();
    this->MoveAndResize(this->x, this->y, this->width, this->height);

    auto clientHeight = this->GetContentHeight();
    auto clientWidth = this->GetContentWidth();

    this->titleLabel->MoveAndResize(0, 0, clientWidth, 1);
    this->shortcuts->MoveAndResize(0, clientHeight - 1, clientWidth, 1);

    int x = 1;
    int y = 2;
    clientWidth -= 2;

    int labelWidth = std::max(TEXT_WIDTH(this->wssPortLabel), TEXT_WIDTH(this->httpPortLabel));
    labelWidth = std::max(labelWidth, TEXT_WIDTH(this->pwLabel));
    int inputWidth = clientWidth - labelWidth - 1;

    this->enableWssCb->MoveAndResize(x, y++, clientWidth, 1);
    this->enableHttpCb->MoveAndResize(x, y++, clientWidth, 1);
    this->enableSyncTransCb->MoveAndResize(x, y++, clientWidth, 1);

    this->wssPortLabel->MoveAndResize(x, y + 1, labelWidth, 1);
    this->wssPortInput->MoveAndResize(labelWidth + 2, y, inputWidth, PORT_INPUT_HEIGHT);
    y += 3;

    this->httpPortLabel->MoveAndResize(x, y + 1, labelWidth, 1);
    this->httpPortInput->MoveAndResize(labelWidth + 2, y, inputWidth, PORT_INPUT_HEIGHT);
    y += 3;

    this->pwLabel->MoveAndResize(x, y + 1, labelWidth, 1);
    this->pwInput->MoveAndResize(labelWidth + 2, y, inputWidth, PORT_INPUT_HEIGHT);
}

void ServerOverlay::Show(Callback callback) {
    Plugin plugin = FindServerPlugin();
    std::shared_ptr<ServerOverlay> overlay(new ServerOverlay(callback, plugin));
    App::Overlays().Push(overlay);
}

std::shared_ptr<IPlugin> ServerOverlay::FindServerPlugin() {
    std::shared_ptr<IPlugin> result;
    using Deleter = PluginFactory::DestroyDeleter<IPlugin>;
    PluginFactory::Instance().QueryInterface<IPlugin, Deleter>(
        "GetPlugin",
        [&result](std::shared_ptr<IPlugin> plugin, const std::string& fn) {
            if (std::string(plugin->Guid()) == WEBSOCKET_PLUGIN_GUID) {
                result = plugin;
            }
        });
    return result;
}

void ServerOverlay::Load() {
    this->enableWssCb->SetChecked(prefs->GetBool(KEY_METADATA_SERVER_ENABLED, false));
    this->enableHttpCb->SetChecked(prefs->GetBool(KEY_AUDIO_SERVER_ENABLED, false));
    this->enableSyncTransCb->SetChecked(prefs->GetBool(KEY_TRANSCODER_SYNCHRONOUS, false));

    this->wssPortInput->SetText(settingIntToString(prefs, KEY_METADATA_SERVER_PORT, 7905));
    this->httpPortInput->SetText(settingIntToString(prefs, KEY_AUDIO_SERVER_PORT, 7906));
    this->pwInput->SetText(prefs->GetString(KEY_PASSWORD, ""));
}

bool ServerOverlay::Save() {
    int wssPort = getIntFromTextInput(this->wssPortInput.get());
    int httpPort = getIntFromTextInput(this->httpPortInput.get());

    if (wssPort <= 0 || httpPort <= 0) {
        return false;
    }

    this->prefs->SetBool(KEY_METADATA_SERVER_ENABLED, this->enableWssCb->IsChecked());
    this->prefs->SetBool(KEY_AUDIO_SERVER_ENABLED, this->enableHttpCb->IsChecked());
    this->prefs->SetBool(KEY_TRANSCODER_SYNCHRONOUS, this->enableSyncTransCb->IsChecked());
    this->prefs->SetInt(KEY_METADATA_SERVER_PORT, wssPort);
    this->prefs->SetInt(KEY_AUDIO_SERVER_PORT, httpPort);
    this->prefs->SetString(KEY_PASSWORD, this->pwInput->GetText().c_str());

    this->prefs->Save();
    this->plugin->Reload();

    return true;
}

bool ServerOverlay::KeyPress(const std::string& key) {
    if (key == "^[") { /* esc closes */
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