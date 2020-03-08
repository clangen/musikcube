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
#include "UpdateCheck.h"
#include <json.hpp>

#include <cursespp/App.h>
#include <cursespp/Window.h>
#include <cursespp/DialogOverlay.h>

#include <core/runtime/Message.h>
#include <core/support/Common.h>

#include <app/util/Messages.h>
#include <app/util/PreferenceKeys.h>
#include <app/version.h>

using namespace nlohmann;
using namespace musik::cube;
using namespace musik::core;
using namespace musik::core::runtime;
using namespace cursespp;

static const std::string UPDATE_CHECK_URL = "https://musikcube.com/version";
static const std::string LATEST = "latest";
static const std::string MAJOR = "major";
static const std::string MINOR = "minor";
static const std::string PATCH = "patch";
static const std::string URL = "url";

#ifdef WIN32
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

static inline int64_t versionCode(short major, short minor, short patch) {
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
        formattedVersion(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH).c_str(),
        PLATFORM.c_str());
}

size_t UpdateCheck::CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    if (ptr && userdata) {
        UpdateCheck* context = static_cast<UpdateCheck*>(userdata);

        if (context->cancel) {
            return 0; /* aborts */
        }

        context->result += std::string(ptr, size * nmemb);
    }
    return size * nmemb;
}

int UpdateCheck::CurlTransferCallback(
    void *ptr, curl_off_t downTotal, curl_off_t downNow, curl_off_t upTotal, curl_off_t upNow)
{
    UpdateCheck* context = static_cast<UpdateCheck*>(ptr);
    if (context->cancel) {
        return -1; /* kill the stream */
    }
    return 0; /* ok! */
}

UpdateCheck::UpdateCheck() {
    this->curl = nullptr;
}

bool UpdateCheck::Run(Callback callback) {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);

    if (this->thread || !callback) {
        return false;
    }

    this->Reset();
    this->callback = callback;

    this->curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, UPDATE_CHECK_URL.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, getUserAgent().c_str());
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3000);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 7500);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 500);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &CurlTransferCallback);

    this->thread.reset(new std::thread([this] {
        bool needsUpdate = false;

        if (curl_easy_perform(this->curl) == CURLE_OK) {
            try {
                json data = json::parse(this->result);
                auto platform = data[LATEST][PLATFORM];
                short major = platform[MAJOR];
                short minor = platform[MINOR];
                short patch = platform[PATCH];

                this->updateUrl = platform[URL];
                this->latestVersion = formattedVersion(major, minor, patch);

                int64_t current = versionCode(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
                int64_t latest = versionCode(major, minor, patch);
                needsUpdate = latest > current;
            }
            catch (...) {
                /* malformed. nothing we can do. */
            }
        }

        cursespp::Window::MessageQueue().Post(
            Message::Create(this, message::UpdateCheckFinished, needsUpdate));
    }));

    return true;
}

void UpdateCheck::Cancel() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);

    if (this->thread) {
        this->cancel = true;
        if (this->thread->joinable()) {
            this->thread->join();
        }
        this->Reset();
    }
}

void UpdateCheck::Reset() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);

    if (this->curl) {
        curl_easy_cleanup(this->curl);
        this->curl = nullptr;
    }

    this->cancel = false;
    this->callback = Callback();
    this->result = "";

    if (this->thread && this->thread->joinable()) {
        this->thread->detach();
    }

    this->thread.reset();
}

void UpdateCheck::ProcessMessage(IMessage &message) {
    if (message.Type() == message::UpdateCheckFinished) {
        auto callback = this->callback;

        this->Reset();

        if (callback) {
            bool updateRequired = message.UserData1() != 0;
            callback(updateRequired, this->latestVersion, this->updateUrl);
        }
    }
}

void UpdateCheck::ShowUpgradeAvailableOverlay(
    const std::string& version, const std::string& url, bool silent)
{
    auto prefs = Preferences::ForComponent("settings");
    std::string prefKey = prefs::keys::LastAcknowledgedUpdateVersion;
    std::string acknowledged = prefs->GetString(prefKey);
    if (!silent || acknowledged != version) {
        std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

        std::string message = u8fmt(
            _TSTR("update_check_dialog_message"),
            version.c_str(), url.c_str());

        (*dialog)
            .SetTitle(_TSTR("update_check_dialog_title"))
            .SetMessage(message);

        dialog->AddButton(
            "o", "o", _TSTR("button_open_url"),
            [url](std::string key) {
                core::OpenFile(url);
            });

        if (silent) {
            dialog->AddButton(
                "x", "x", _TSTR("button_dont_remind_me"),
                [prefs, prefKey, version](std::string key) {
                    prefs->SetString(prefKey.c_str(), version.c_str());
                    prefs->Save();
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
        std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

        (*dialog)
            .SetTitle(_TSTR("update_check_no_updates_title"))
            .SetMessage(_TSTR("update_check_no_updates_message"))
            .AddButton("KEY_ENTER", "ENTER", _TSTR("button_close"));

        App::Overlays().Push(dialog);
}
