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
using namespace musik::cube;
using namespace cursespp;

static const std::string WEBSOCKET_PLUGIN_GUID = "9fc897a3-dfd5-4524-a0fc-b02f46aea4a9";
static const char* KEY_METADATA_SERVER_ENABLED = "websocket_server_enabled";
static const char* KEY_METADATA_SERVER_PORT = "websocket_server_port";
static const char* KEY_AUDIO_SERVER_ENABLED = "http_server_enabled";
static const char* KEY_AUDIO_SERVER_PORT = "http_server_port";
static const char* KEY_TRANSCODER_CACHE_COUNT = "transcoder_cache_count";
static const char* KEY_TRANSCODER_SYNCHRONOUS = "transcoder_synchronous";
static const char* KEY_USE_IPV6 = "use_ipv6";
static const char* KEY_PASSWORD = "password";

#define VERTICAL_PADDING 2
#define DEFAULT_HEIGHT 18
#define DEFAULT_WIDTH 45

#define RIGHT(x) (x->GetX() + x->GetWidth())
#define TEXT_WIDTH(x) ((int) u8cols(x->GetText()))

using Callback = ServerOverlay::Callback;
using Prefs = ServerOverlay::Prefs;

static std::string settingIntToString(Prefs prefs, const std::string& key, int defaultValue) {
    return std::to_string(prefs->GetInt(key, defaultValue));
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
    this->InitViews();
    this->Load();
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

    this->ipv6Cb->MoveAndResize(x, y++, clientWidth, 1);

    const int transCcacheLabelWidth = TEXT_WIDTH(transCacheLabel);
    this->enableSyncTransCb->MoveAndResize(x, y++, clientWidth, 1);
    this->transCacheLabel->MoveAndResize(x, y, transCcacheLabelWidth, 1);
    this->transCacheInput->MoveAndResize(x + transCcacheLabelWidth + 1, y, 5, 1);
    y += 2;

    const int pwLabelWidth = TEXT_WIDTH(pwLabel);
    this->pwLabel->MoveAndResize(x, y, pwLabelWidth, 1);
    this->pwInput->MoveAndResize(pwLabelWidth + 2, y, clientWidth - pwLabelWidth - 1, 1);
}

void ServerOverlay::Show(Callback callback) {
    Plugin plugin = FindServerPlugin();
    std::shared_ptr<ServerOverlay> overlay(new ServerOverlay(callback, plugin));
    App::Overlays().Push(overlay);
}

std::shared_ptr<IPlugin> ServerOverlay::FindServerPlugin() {
    return PluginFactory::Instance().QueryGuid(WEBSOCKET_PLUGIN_GUID);
}

void ServerOverlay::Load() {
    this->enableWssCb->SetChecked(prefs->GetBool(KEY_METADATA_SERVER_ENABLED, false));
    this->enableHttpCb->SetChecked(prefs->GetBool(KEY_AUDIO_SERVER_ENABLED, false));
    this->enableSyncTransCb->SetChecked(prefs->GetBool(KEY_TRANSCODER_SYNCHRONOUS, false));

    this->wssPortInput->SetText(settingIntToString(prefs, KEY_METADATA_SERVER_PORT, 7905));
    this->httpPortInput->SetText(settingIntToString(prefs, KEY_AUDIO_SERVER_PORT, 7906));
    this->ipv6Cb->SetChecked(prefs->GetBool(KEY_USE_IPV6, false));
    this->transCacheInput->SetText(settingIntToString(prefs, KEY_TRANSCODER_CACHE_COUNT, 50));
    this->pwInput->SetText(prefs->GetString(KEY_PASSWORD, ""));
}

bool ServerOverlay::Save() {
    int wssPort = getIntFromTextInput(this->wssPortInput.get());
    int httpPort = getIntFromTextInput(this->httpPortInput.get());
    int cacheCount = getIntFromTextInput(this->transCacheInput.get());

    if (wssPort <= 0 || httpPort <= 0 || cacheCount < 0) {
        return false;
    }

    this->prefs->SetBool(KEY_METADATA_SERVER_ENABLED, this->enableWssCb->IsChecked());
    this->prefs->SetBool(KEY_AUDIO_SERVER_ENABLED, this->enableHttpCb->IsChecked());
    this->prefs->SetBool(KEY_USE_IPV6, this->ipv6Cb->IsChecked());
    this->prefs->SetBool(KEY_TRANSCODER_SYNCHRONOUS, this->enableSyncTransCb->IsChecked());
    this->prefs->SetInt(KEY_METADATA_SERVER_PORT, wssPort);
    this->prefs->SetInt(KEY_AUDIO_SERVER_PORT, httpPort);
    this->prefs->SetInt(KEY_TRANSCODER_CACHE_COUNT, cacheCount);
    this->prefs->SetString(KEY_PASSWORD, this->pwInput->GetText().c_str());

    this->prefs->Save();
    this->plugin->Reload();

    return true;
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