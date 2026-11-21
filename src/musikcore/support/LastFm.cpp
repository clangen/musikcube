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

#include "pch.hpp"

#include "LastFm.h"
#include "OnlineTransfer.h"

#include <musikcore/support/Preferences.h>
#include <musikcore/sdk/HttpClient.h>
#include <musikcore/support/PreferenceKeys.h>

#include <sstream>
#include <map>

#pragma warning(push, 0)
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#pragma warning(pop)

extern "C" {
    #include <md5.h>
}

/* http://www.last.fm/group/Last.fm+Web+Services/forum/21604/_/522900 -- it's ok to
put our key in the code */
static const std::string API_KEY = "8b7b7369cd92bbcc071c5f8a1de1d287";
static const std::string API_SECRET = "6dc09da925fe5c115b90320213c53b46";
static const std::string URL_BASE = "http://ws.audioscrobbler.com/2.0/";
static const std::string GET_TOKEN = "auth.getToken";
static const std::string GET_SESSION = "auth.getSession";
static const std::string UPDATE_NOW_PLAYING = "track.updateNowPlaying";
static const std::string UPDATE_RECENT = "track.scrobble";
static const std::string ACCOUNT_LINK_URL_BASE = "http://www.last.fm/api/auth/?api_key=" + API_KEY + "&token=";

using namespace musik;
using namespace musik::core::prefs;

using LastFmClient = musik::core::sdk::HttpClient<std::stringstream>;
using Preferences = musik::core::Preferences;
using Prefs = std::shared_ptr<Preferences>;

static std::shared_ptr<LastFmClient> createClient() {
    return LastFmClient::Create(std::stringstream());
}

static void validate(musik::core::lastfm::Session& session) {
    session.valid =
    session.sessionId.size() &&
    session.username.size() &&
    session.token.size();
}

static std::string encode(std::string value) {
    static CURL* curl = curl_easy_init();
    if (curl && value.c_str()) {
        char* encoded = curl_easy_escape(curl, value.c_str(), (int) value.size());
        if (encoded) {
            value = encoded;
            curl_free(encoded);
        }
    }
    return value;
}

static std::string gernateSignedUrlParams(
    const std::string& method,
    std::map<std::string, std::string>&& params = {})
{
    params["method"] = method;
    params["api_key"] = API_KEY;

    std::string toHash;
    std::string urlParams;
    bool first = true;

    for (auto it : params) {
        toHash += it.first + it.second;
        urlParams += (first ? "" : "&") + it.first + "=" + encode(it.second);
        first = false;
    }

    toHash += API_SECRET;

    /* compute the sum */
    EX_MD5_CTX context = {};
    unsigned char rawDigest[16];
    Ex_MD5_Init(&context);
    Ex_MD5_Update(&context, (const void*)toHash.c_str(), (unsigned long) toHash.length());
    Ex_MD5_Final(rawDigest, &context);

    /* convert to hex */
    char hexDigest[33];
    for (size_t i = 0; i < 16; i++) {
        snprintf(&(hexDigest[i * 2]), 16 * 2, "%02x", (unsigned int)rawDigest[i]);
    }
    hexDigest[32] = 0;

    urlParams += "&format=json&api_sig=" + std::string(hexDigest);
    return urlParams;
}

static std::string generateSignedUrl(
    const std::string& method,
    std::map<std::string, std::string>&& params = { })
{
    return URL_BASE + "?" + gernateSignedUrlParams(method, std::move(params));
}

static std::mutex sessionMutex;

static inline Prefs settings() {
    return Preferences::ForComponent(components::Settings);
}

namespace musik { namespace core { namespace lastfm {

    void CreateAccountLinkToken(const std::atomic_bool& cancelled, TokenCallback callback) {
        std::string url = generateSignedUrl(GET_TOKEN);

        auto client = createClient();
        client->Url(url)
            .Mode(LastFmClient::Thread::Current)
            .Decorator([&](CURL* curl) { ConfigureOnlineTransfer(curl, cancelled); })
            .Run([callback](LastFmClient* client, int statusCode, CURLcode curlCode) {
                std::string token;
                if (statusCode == 200) {
                    try {
                        auto json = nlohmann::json::parse(client->Stream().str());
                        token = json.value("token", "");
                    }
                    catch (...) {
                        /* not much we can do... */
                    }
                }
                callback(std::move(token));
        });
    }

    extern void CreateSession(const std::string& token, const std::atomic_bool& cancelled, SessionCallback callback) {
        std::string url = generateSignedUrl(GET_SESSION, { { "token", token } });

        auto client = createClient();
        client->Url(url)
            .Mode(LastFmClient::Thread::Current)
            .Decorator([&](CURL* curl) { ConfigureOnlineTransfer(curl, cancelled); })
            .Run([token, callback](LastFmClient* client, int statusCode, CURLcode curlCode) {
                Session session;
                session.token = token;

                if (statusCode == 200) {
                    try {
                        auto json = nlohmann::json::parse(client->Stream().str());
                        auto subscriber = json["session"];
                        session.username = subscriber.value("name", "");
                        session.sessionId = subscriber.value("key", "");
                    }
                    catch (...) {
                        /* not much we can do... */
                    }
                }

                validate(session);
                callback(session);
        });
    }

    const std::string CreateAccountLinkUrl(const std::string& token) {
        return ACCOUNT_LINK_URL_BASE + token;
    }

    Session LoadSession() {
        std::lock_guard lock(sessionMutex);
        namespace keys = core::prefs::keys;
        auto prefs = settings();
        Session session;
        session.token = prefs->GetString(keys::LastFmToken);
        session.sessionId = prefs->GetString(keys::LastFmSessionId);
        session.username = prefs->GetString(keys::LastFmUsername);
        validate(session);
        return session;
    }

    bool SaveSession(const Session& session) {
        std::lock_guard lock(sessionMutex);
        return settings()->SetValues({
            { keys::LastFmToken, session.token },
            { keys::LastFmSessionId, session.sessionId },
            { keys::LastFmUsername, session.username }
        }, true);
    }

    bool ClearSession() {
        return SaveSession({});
    }

    PlaybackReports::~PlaybackReports() {
        this->Stop();
    }

    void PlaybackReports::Stop() {
        std::vector<std::future<void>> transfers;
        {
            std::lock_guard lock(this->mutex);
            this->cancelled.store(true);
            transfers.swap(this->transfers);
        }
        /* Destruction of the saved futures joins the transfers outside the lock. */
    }

    void PlaybackReports::Scrobble(const TrackPtr& track) {
        this->Submit(track, true);
    }

    void PlaybackReports::UpdateNowPlaying(const TrackPtr& track) {
        this->Submit(track, false);
    }

    void PlaybackReports::Submit(const TrackPtr& track, bool scrobble) {
        if (!track || this->cancelled.load()) {
            return;
        }
        const auto session = LoadSession();
        if (!session.valid) {
            return;
        }

        std::map<std::string, std::string> fields {
            { "track", track->GetString("title") },
            { "album", track->GetString("album") },
            { "artist", track->GetString("artist") },
            { "albumArtist", track->GetString("album_artist") },
            { "trackNumber", track->GetString("track") },
            { "sk", session.sessionId }
        };
        if (scrobble) {
            fields.emplace("timestamp", std::to_string(std::time(nullptr)));
        }
        else {
            fields.emplace("duration", track->GetString("duration"));
        }

        std::lock_guard lock(this->mutex);
        if (this->cancelled.load()) {
            return;
        }
        std::erase_if(this->transfers, [](const auto& transfer) {
            return transfer.wait_for(std::chrono::seconds::zero()) == std::future_status::ready;
        });
        auto body = gernateSignedUrlParams(scrobble ? UPDATE_RECENT : UPDATE_NOW_PLAYING, std::move(fields));
        this->transfers.push_back(std::async(std::launch::async, [this, body = std::move(body)] {
            if (!this->cancelled.load()) {
                createClient()->Url(URL_BASE)
                    .Mode(LastFmClient::Thread::Current)
                    .Decorator([this](CURL* curl) { ConfigureOnlineTransfer(curl, this->cancelled); })
                    .Header("Content-Type", "application/x-www-form-urlencoded")
                    .Method(LastFmClient::HttpMethod::Post)
                    .PostBody(body)
                    .Run();
            }
        }));
    }

} } }

