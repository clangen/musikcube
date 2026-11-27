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
#include <app/util/OpenUrl.h>

#include <musikcore/settings.hpp>
#include "UpdateCheck.h"

#include <nlohmann/json.hpp>

#include <cursespp/App.h>
#include <cursespp/Window.h>
#include <cursespp/DialogOverlay.h>

#include <common/runtime/Message.h>
#include <common/HttpClient.h>
#include <common/version.h>

#include <app/util/Messages.h>
#include <app/util/SettingKeys.h>

#include <sstream>
#include <utility>

using namespace nlohmann;
using namespace musik::cube;
using namespace musik::core;
using namespace musik::core::runtime;
using namespace cursespp;

static const std::string UPDATE_CHECK_URL = "https://musikcube.com/version";
static constexpr long UPDATE_CHECK_TIMEOUT_MS = 5000;
static const std::string LATEST = "latest";
static const std::string MAJOR = "major";
static const std::string MINOR = "minor";
static const std::string PATCH = "patch";
static const std::string URL = "url";

#ifdef _WIN64
static const std::string PLATFORM = "win64";
#elif WIN32
static const std::string PLATFORM = "win32";
#elif defined __APPLE__
static const std::string PLATFORM = "macos";
#elif defined __FreeBSD__
static const std::string PLATFORM = "freebsd";
#elif defined __OpenBSD__
static const std::string PLATFORM = "openbsd";
#else
static const std::string PLATFORM = "linux";
#endif

constexpr int64_t versionCode(short major, short minor, short patch) noexcept {
    int64_t version = major;
    version = (version << 16) | minor;
    version = (version << 16) | patch;
    return version;
}

static inline std::string formattedVersion(short major, short minor, short patch) {
    return u8fmt("%d.%d.%d", major, minor, patch);
}

static inline std::string getUserAgent() {
    return u8fmt("musikcube %s (%s)",
        formattedVersion(MUSIKCUBE_VERSION_MAJOR, MUSIKCUBE_VERSION_MINOR, MUSIKCUBE_VERSION_PATCH).c_str(),
        PLATFORM.c_str());
}

UpdateCheck::UpdateCheck() {
    Window::MessageQueue().Register(this);
}

UpdateCheck::~UpdateCheck() {
    this->Cancel();
    Window::MessageQueue().Unregister(this);
}

bool UpdateCheck::Run(Callback callback) {
    if (!callback) {
        return false;
    }

    this->Cancel();
    this->callback = std::move(callback);

    /* own the transfer so cancellation can join it before process shutdown. */
    this->worker = std::jthread([this](std::stop_token stop) {
        using HttpClient = musik::core::sdk::HttpClient<std::stringstream>;
        auto client = HttpClient::Create(std::stringstream());
        client->Url(UPDATE_CHECK_URL)
            .UserAgent(getUserAgent())
            .Mode(HttpClient::Thread::Current)
            .Timeout(UPDATE_CHECK_TIMEOUT_MS)
            .Cancellation(stop)
            .Run([this, &stop](HttpClient* client, int, bool success) {
                if (stop.stop_requested()) {
                    return;
                }

                bool needsUpdate = false;
                if (success) {
                    try {
                        const auto platform = json::parse(client->Stream().str())[LATEST][PLATFORM];
                        const short major = platform[MAJOR];
                        const short minor = platform[MINOR];
                        const short patch = platform[PATCH];
                        this->updateUrl = platform[URL].get<std::string>();
                        this->latestVersion = formattedVersion(major, minor, patch);

                        constexpr auto current = versionCode(
                            MUSIKCUBE_VERSION_MAJOR, MUSIKCUBE_VERSION_MINOR, MUSIKCUBE_VERSION_PATCH);
                        needsUpdate = versionCode(major, minor, patch) > current;
                    }
                    catch (...) {
                        /* a malformed response contains no usable update. */
                    }
                }

                Window::MessageQueue().Post(Message::Create(this, message::UpdateCheckFinished, needsUpdate));
            });
    });

    return true;
}

void UpdateCheck::Cancel() {
    /* join before removing queued completions or letting OpenSSL shut down. */
    if (this->worker.joinable()) {
        this->worker.request_stop();
        this->worker.join();
    }

    Window::MessageQueue().Remove(this, message::UpdateCheckFinished);
    this->callback = {};
}

void UpdateCheck::ProcessMessage(IMessage &message) {
    if (message.Type() == message::UpdateCheckFinished) {
        auto callback = std::move(this->callback);
        this->Cancel();

        if (callback) {
            const bool updateRequired = message.UserData1() != 0;
            callback(updateRequired, this->latestVersion, this->updateUrl);
        }
    }
}

void UpdateCheck::ShowUpgradeAvailableOverlay(
    const std::string& version, const std::string& url, bool silent)
{
    auto prefs = api::Settings { MC_SETTINGS_APPLICATION };
    std::string prefKey = prefs::keys::LastAcknowledgedUpdateVersion;
    std::string acknowledged = prefs.GetString(prefKey);
    if (!silent || acknowledged != version) {
        auto const dialog = std::make_shared<DialogOverlay>();

        std::string message = u8fmt(
            _TSTR("update_check_dialog_message"),
            version.c_str(), url.c_str());

        (*dialog)
            .SetTitle(_TSTR("update_check_dialog_title"))
            .SetMessage(message);

        dialog->AddButton(
            "o", "o", _TSTR("button_open_url"),
            [url](std::string key) {
                musik::cube::OpenUrl(url);
            });

        if (silent) {
            dialog->AddButton(
                "x", "x", _TSTR("button_dont_remind_me"),
                [prefs, prefKey, version](std::string key) mutable {
                    prefs.SetString(prefKey.c_str(), version.c_str());
                    prefs.Save();
                });

            dialog->AddButton("l", "l", _TSTR("button_remind_me_later"));
        }
        else {
            dialog->AddButton("KEY_ENTER", "ENTER", _TSTR("ok"));
        }

        App::Overlays().Push(dialog);
    }
}

void UpdateCheck::ShowNoUpgradeFoundOverlay() {
    auto const dialog = std::make_shared<DialogOverlay>();

    (*dialog)
        .SetTitle(_TSTR("update_check_no_updates_title"))
        .SetMessage(_TSTR("update_check_no_updates_message"))
        .AddButton("KEY_ENTER", "ENTER", _TSTR("button_close"));

    App::Overlays().Push(dialog);
}
