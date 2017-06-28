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
#include "UpdateCheck.h"
#include <json.hpp>

#include <cursespp/Window.h>
#include <app/util/Messages.h>
#include <app/util/Version.h>
#include <core/runtime/Message.h>

using namespace nlohmann;
using namespace musik::box;
using namespace musik::core::runtime;

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
    return boost::str(boost::format("%d.%d.%d") % major % minor % patch);
}

size_t UpdateCheck::curlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    if (ptr && userdata) {
        UpdateCheck* context = static_cast<UpdateCheck*>(userdata);

        if (context->cancel) {
            return 0; /* aborts */
        }

        context->result += std::string(ptr, size * nmemb);
    }
    return size * nmemb;
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
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "musikcube UpdateCheck");
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3000);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 7500);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 500);

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