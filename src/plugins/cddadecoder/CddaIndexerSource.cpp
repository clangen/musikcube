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

#include "stdafx.h"

#include "CddaIndexerSource.h"

#include <core/sdk/IIndexerNotifier.h>

#include <curl/curl.h>

#include <string>
#include <sstream>
#include <set>
#include <map>

#include <boost/algorithm/string.hpp>

using namespace musik::core::sdk;

struct CddbMetadata {
    std::string album;
    std::string artist;
    std::string year;
    std::string genre;
    std::vector<std::string> titles;
};

using DiscList = std::vector<CddaDataModel::AudioDiscPtr>;
using DiscIdList = std::set<std::string>;

static std::mutex globalStateMutex;
static musik::core::sdk::IIndexerNotifier* notifier;

static const std::string FREEDB_URL = "http://freedb.freedb.org/~cddb/cddb.cgi";
static const std::string FREEDB_HELLO = "&hello=user+musikcube+cddadecoder+0.5.0&proto=6";
static std::map<std::string, std::shared_ptr<CddbMetadata>> discIdToMetadata;

extern "C" __declspec(dllexport) void SetIndexerNotifier(musik::core::sdk::IIndexerNotifier* notifier) {
    std::unique_lock<std::mutex> lock(globalStateMutex);
    ::notifier = notifier;
}

static std::vector<std::string> tokenize(const std::string& str, char delim = '/') {
    std::vector<std::string> result;
    std::istringstream iss(str);
    std::string token;

    while (std::getline(iss, token, delim)) {
        result.push_back(token);
    }

    return result;
}

static std::string createExternalId(const char driveLetter, const std::string& cddbId, int track) {
    return "audiocd/" + std::string(1, driveLetter) + "/" + cddbId + "/" + std::to_string(track);
}

static bool exists(DiscIdList& discs, CddaDataModel& model, const std::string& externalId) {
    std::vector<std::string> tokens = tokenize(externalId);

    if (tokens.size() < 4) { /* see format above */
        return false;
    }

    /* find by drive letter. */
    auto disc = model.GetAudioDisc(tolower(tokens.at(1)[0]));

    if (!disc) {
        return false;
    }

    /* make sure the track is an audio track */
    try {
        int trackNumber = std::stoi(tokens.at(3));
        if (disc->GetTrackAt(trackNumber)->GetType() != CddaDataModel::DiscTrack::Type::Audio) {
            return false;
        }
    }
    catch (...) {
        return false; /* track number parse failed? malformed. reject. */
    }

    return discs.find(tokens.at(2)) != discs.end();
}

static std::string labelForDrive(const char driveLetter) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "[audio cd %c:\\]", tolower(driveLetter));
    return std::string(buffer);
}

static size_t curlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    if (ptr && userdata) {
        std::string& str = *(reinterpret_cast<std::string*>(userdata));
        str += std::string(ptr, size * nmemb);
    }
    return size * nmemb;
}

static CURL* newCurlEasy(const std::string& url, void* userdata) {
    CURL* curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "musikcube CddaIndexerSource");
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, userdata);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3000);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 7500);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 500);

    //curl_easy_setopt (curl, CURLOPT_PROXY, "localhost");
    //curl_easy_setopt (curl, CURLOPT_PROXYPORT, 8080);

    // if (useproxy) {
    //     curl_easy_setopt (this->curlEasy, CURLOPT_PROXY, proxyaddress);
    //     curl_easy_setopt (this->curlEasy, CURLOPT_PROXYUSERPWD, proxyuserpass);
    // }

    return curl;
}

static void cddbLookup(const std::string& discId, std::string listQueryParams) {
    listQueryParams = listQueryParams + FREEDB_HELLO;
    std::string listResponse;

    /* get a listing of all entries for the specified disc */
    CURL* listing = newCurlEasy(FREEDB_URL, static_cast<void*>(&listResponse));
    curl_easy_setopt(listing, CURLOPT_POSTFIELDSIZE, listQueryParams.size());
    curl_easy_setopt(listing, CURLOPT_POSTFIELDS, listQueryParams.c_str());

    CURLcode result = curl_easy_perform(listing);
    curl_easy_cleanup(listing);

    std::string discQueryParams;

    if (result == CURLE_OK) { /* well... we got something back */
        listResponse = boost::replace_all_copy(listResponse, "\r\n", "\n");

        std::vector<std::string> lines;
        boost::algorithm::split(lines, listResponse, boost::is_any_of("\n"));

        /* just choose the first disc for now. we don't have a way to present a
        UI to the user, so this is really all we can do. */
        if (lines.size() >= 1) {
            if (lines.at(0).find("200") == 0) {
                std::vector<std::string> parts;
                boost::algorithm::split(parts, lines.at(0), boost::is_any_of(" "));

                if (parts.size() >= 3) {
                    discQueryParams = "cmd=cddb+read+" + parts[1] + "+" + parts[2] + FREEDB_HELLO;
                }
            }
            /* the first line of the response has a status code. anything
            in the 200 range is fine. */
            else if (lines.at(0).find("21") == 0) {
                std::vector<std::string> parts;
                boost::algorithm::split(parts, lines.at(1), boost::is_any_of(" "));

                if (parts.size() >= 2) {
                    discQueryParams = "cmd=cddb+read+" + parts[0] + "+" + parts[1] + FREEDB_HELLO;
                }
            }
        }
    }

    /* we resolved at least one disc. let's look it up. */
    if (discQueryParams.size()) {
        std::string discResponse;
        CURL* details = newCurlEasy(FREEDB_URL, static_cast<void*>(&discResponse));
        curl_easy_setopt(details, CURLOPT_POSTFIELDSIZE, discQueryParams.size());
        curl_easy_setopt(details, CURLOPT_POSTFIELDS, discQueryParams.c_str());

        CURLcode result = curl_easy_perform(details);
        curl_easy_cleanup(details);

        if (result == CURLE_OK) {
            discResponse = boost::replace_all_copy(discResponse, "\r\n", "\n");

            std::vector<std::string> lines;
            boost::algorithm::split(lines, discResponse, boost::is_any_of("\n"));

            std::shared_ptr<CddbMetadata> metadata(new CddbMetadata());

            for (auto line : lines) {
                auto len = line.size();
                if (len) {
                    auto eq = line.find_first_of('=');
                    if (eq != std::string::npos) {
                        std::string key = boost::trim_copy(line.substr(0, eq));
                        std::string value = boost::trim_copy(line.substr(eq + 1));

                        if (key == "DTITLE") {
                            auto slash = value.find_first_of('/');
                            std::string artist, album;

                            if (slash == std::string::npos) {
                                artist = album = value;
                            }
                            else {
                                artist = boost::trim_copy(value.substr(0, slash));
                                album = boost::trim_copy(value.substr(slash + 1));
                            }

                            metadata->artist = artist;
                            metadata->album = album;
                        }
                        else if (key == "DYEAR") {
                            metadata->year = value;
                        }
                        else if (key == "DGENRE") {
                            metadata->genre = value;
                        }
                        else if (key.find("TTITLE") == 0) {
                            metadata->titles.push_back(value);
                        }
                    }
                }
            }

            /* done parsing... */
            if (discId.size()) {
                discIdToMetadata[discId] = metadata;
            }
        }
    }
}

CddaIndexerSource::CddaIndexerSource()
: model(CddaDataModel::Instance()) {
    model.AddEventListener(this);
}

CddaIndexerSource::~CddaIndexerSource() {
    model.RemoveEventListener(this);
}

void CddaIndexerSource::Release() {
    delete this;
}

void CddaIndexerSource::RefreshModel() {
    this->discs = model.GetAudioDiscs();
    discIds.clear();
    for (auto disc : discs) {
        discIds.insert(disc->GetCddbId());
    }
}

void CddaIndexerSource::OnAudioDiscInsertedOrRemoved() {
    std::unique_lock<std::mutex> lock(globalStateMutex);
    if (::notifier) {
        ::notifier->ScheduleRescan(this);
    }
}

void CddaIndexerSource::OnBeforeScan() {
    this->RefreshModel();
}

void CddaIndexerSource::OnAfterScan() {
    /* nothing to do... */
}

ScanResult CddaIndexerSource::Scan(
    IIndexerWriter* indexer,
    const char** indexerPaths,
    unsigned indexerPathsCount)
{
    using namespace std::placeholders;

    for (auto disc : this->discs) {
        char driveLetter = disc->GetDriveLetter();
        std::string cddbId = disc->GetCddbId();
        std::shared_ptr<CddbMetadata> metadata = nullptr;

        {
            std::unique_lock<std::mutex> lock(globalStateMutex);
            auto it = discIdToMetadata.find(cddbId);

            if (it == discIdToMetadata.end()) {
                try {
                    cddbLookup(cddbId, disc->GetCddbQueryString()); /* it'll time out in a few seconds */
                    it = discIdToMetadata.find(cddbId);
                }
                catch (...) {
                    /* this should never happen. */
                }
            }

            metadata = (it != discIdToMetadata.end()) ? it->second : nullptr;
        }

        std::string label = labelForDrive(driveLetter);
        std::string album = metadata ? "[CD] " + metadata->album : label;
        std::string artist = metadata ? "[CD] " + metadata->artist : label;
        std::string genre = metadata ? "[CD] " + metadata->genre : label;

        for (int i = 0; i < disc->GetTrackCount(); i++) {
            auto discTrack = disc->GetTrackAt(i);

            if (discTrack->GetType() == CddaDataModel::DiscTrack::Type::Audio) {
                auto externalId = createExternalId(driveLetter, cddbId, i);
                auto track = indexer->CreateWriter();

                track->SetValue("album", album.c_str());
                track->SetValue("artist", artist.c_str());
                track->SetValue("album_artist", artist.c_str());
                track->SetValue("genre", genre.c_str());
                track->SetValue("filename", discTrack->GetFilePath().c_str());
                track->SetValue("duration", std::to_string((int)round(discTrack->GetDuration())).c_str());
                track->SetValue("track", std::to_string(i + 1).c_str());

                if (metadata) {
                    track->SetValue("title", metadata->titles.at(i).c_str());
                }
                else {
                    std::string title = "track #" + std::to_string(i + 1);
                    track->SetValue("title", title.c_str());
                }

                indexer->Save(this, track, externalId.c_str());

                track->Release();
            }
        }
    }

    return ScanCommit;
}

void CddaIndexerSource::Interrupt() {

}

void CddaIndexerSource::ScanTrack(
    IIndexerWriter* indexer,
    ITagStore* tagStore,
    const char* externalId)
{
    if (!exists(this->discIds, this->model, externalId)) {
        indexer->RemoveByExternalId(this, externalId);
    }
}

int CddaIndexerSource::SourceId() {
    return std::hash<std::string>()(PLUGIN_NAME);
}