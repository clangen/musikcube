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

#include "CddaIndexerSource.h"

#include <string>
#include <sstream>
#include <set>

using namespace musik::core::sdk;

using DiscList = std::vector<CddaDataModel::AudioDiscPtr>;
using DiscIdList = std::set<std::string>;

static std::mutex globalSinkMutex;
static musik::core::sdk::IIndexerSink* globalSink;

extern "C" __declspec(dllexport) void SetIndexerSink(musik::core::sdk::IIndexerSink* sink) {
    std::unique_lock<std::mutex> lock(globalSinkMutex);
    ::globalSink = sink;
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

static bool exists(DiscIdList& discs, const std::string& externalId) {
    std::vector<std::string> tokens = tokenize(externalId);

    if (tokens.size() < 4) { /* see format above */
        return false;
    }

    return discs.find(tokens.at(2)) != discs.end();
}

CddaIndexerSource::CddaIndexerSource()
: model(CddaDataModel::Instance()) {
    model.AddEventListener(this);
}

CddaIndexerSource::~CddaIndexerSource() {
    model.RemoveEventListener(this);
}

void CddaIndexerSource::Destroy() {
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
    std::unique_lock<std::mutex> lock(globalSinkMutex);
    if (::globalSink) {
        ::globalSink->Rescan(this);
    }
}

void CddaIndexerSource::OnBeforeScan() {
    this->RefreshModel();
}

void CddaIndexerSource::OnAfterScan() {
    /* nothing to do... */
}

void CddaIndexerSource::Scan(musik::core::sdk::IIndexerSink* indexer) {
    for (auto disc : this->discs) {
        char driveLetter = disc->GetDriveLetter();
        std::string cddbId = disc->GetCddbId();

        for (int i = 0; i < disc->GetTrackCount(); i++) {
            auto discTrack = disc->GetTrackAt(i);

            std::string title = "[CD AUDIO] track #" + std::to_string(i + 1);
            std::string externalId = createExternalId(driveLetter, cddbId, i);

            auto track = indexer->CreateWriter();

            track->SetValue("album", "[CD AUDIO] album");
            track->SetValue("artist", "[CD AUDIO] artist");
            track->SetValue("album_artist", "[CD AUDIO] album artist");
            track->SetValue("genre", "[CD AUDIO] genre");
            track->SetValue("title", title.c_str());
            track->SetValue("filename", discTrack->GetFilePath().c_str());
            track->SetValue("duration", std::to_string((int) round(discTrack->GetDuration())).c_str());
            track->SetValue("track", std::to_string(i + 1).c_str());

            indexer->Save(this, track, externalId.c_str());

            track->Release();
        }
    }

    discIds.clear();
}

void CddaIndexerSource::Scan(
    IIndexerSink* indexer,
    IRetainedTrackWriter* track,
    const char* externalId)
{
    if (!exists(this->discIds, externalId)) {
        indexer->RemoveByExternalId(this, externalId);
    }
}

int CddaIndexerSource::SourceId() {
    return std::hash<std::string>()(PLUGIN_NAME);
}