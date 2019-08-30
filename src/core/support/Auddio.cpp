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

#include "pch.hpp"
#include "Auddio.h"
#include "Common.h"
#include <core/sdk/HttpClient.h>
#include <core/support/Preferences.h>
#include <core/support/PreferenceKeys.h>
#include <curl/curl.h>
#include <sstream>
#include <json.hpp>

/* https://api.audd.io/findLyrics/?q=the%20beatles%20sgt%20pepper%20reprise */

using namespace musik::core;
using namespace musik::core::prefs;

using AuddioClient = musik::core::sdk::HttpClient<std::stringstream>;

static std::shared_ptr<AuddioClient> createClient() {
    return AuddioClient::Create(std::stringstream());
}

static std::string encode(std::string value) {
    static CURL* curl = curl_easy_init();
    if (curl && value.c_str()) {
        char* encoded = curl_easy_escape(curl, value.c_str(), value.size());
        if (encoded) {
            value = encoded;
            curl_free(encoded);
        }
    }
    return value;
}

static std::string getApiToken() {
    auto prefs = Preferences::ForComponent(components::Settings);
    return prefs->GetString(keys::AuddioApiToken);
}

namespace musik { namespace core { namespace auddio {
    bool Available() {
        return getApiToken().size() > 0;
    }

    void FindLyrics(TrackPtr track, LyricsCallback callback) {
        std::string apiToken = getApiToken();

        if (!apiToken.size()) {
            callback(track, "");
            return;
        }

        std::string artist = encode(track->GetString("artist"));
        std::string title = encode(track->GetString("title"));
        std::string url =
            "https://api.audd.io/findLyrics/?q=" +
            artist + "%20" + title +
            "&api_token=" + apiToken;

        auto client = createClient();
        client->Url(url)
            .Mode(AuddioClient::Thread::Background)
            .Run([track, callback](AuddioClient* client, int statusCode, CURLcode curlCode) {
                std::string lyrics;
                if (statusCode == 200) {
                    try {
                        std::string response = client->Stream().str();
                        auto json = nlohmann::json::parse(response);
                        if (json.value("status", "") == "success") {
                            lyrics = json["result"][0]["lyrics"];
                        }
                    }
                    catch (...) {
                        /* not much we can do... */
                    }
                }

                callback(track, lyrics);
        });
    }
} } }