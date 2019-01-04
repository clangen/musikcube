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

#include "Constants.h"
#include "GmeIndexerSource.h"
#include <string>
#include <sstream>
#include <set>
#include <map>
#include <gme.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

using namespace musik::core::sdk;

static std::string getM3uFor(const std::string& fn) {
    size_t lastDot = fn.find_last_of(".");
    if (lastDot != std::string::npos) {
        std::string m3u = fn.substr(0, lastDot) + ".m3u";
        if (access(m3u.c_str(), R_OK) != -1) {
            return m3u;
        }
    }
    return "";
}

static bool exists(const std::string& externalId) {
    std::string fn;
    int trackNum;
    if (!parseExternalId(externalId, fn, trackNum)) {
        return false;
    }
    return access(fn.c_str(), R_OK) != -1;
}

static void updateMetadata(
    const std::string& fn,
    IIndexerSource* source,
    IIndexerWriter* indexer)
{
    gme_t* data = nullptr;
    gme_err_t err = gme_open_file(fn.c_str(), &data, 44100);
    if (err) {
        std::cerr << "error opening file: " << err << "\n";
    }
    else {
        std::string m3u = getM3uFor(fn);
        if (m3u.size()) {
            err = gme_load_m3u(data, m3u.c_str());
            if (err) {
                std::cerr << "m3u found, but load failed: " << err << "\n";
            }
        }

        for (int i = 0; i < gme_track_count(data); i++) {
            gme_info_t* info = nullptr;
            err = gme_track_info(data, &info, i);
            if (err) {
                std::cerr << "error getting track: " << err << "\n";
            }
            else if (info) {
                auto track = indexer->CreateWriter();

                const std::string trackNum = std::to_string(i + 1).c_str();
                const std::string defaultTitle = "Track " + std::to_string(1 + i);
                const std::string duration = std::to_string((float) info->play_length / 1000.0f);
                const std::string externalId = createExternalId(fn, i);

                track->SetValue("album", info->game);
                track->SetValue("album_artist", info->system);
                track->SetValue("genre", info->system);
                track->SetValue("track", trackNum.c_str());
                track->SetValue("duration", duration.c_str());
                track->SetValue("filename", externalId.c_str());

                if (strlen(info->author)) {
                    track->SetValue("artist", info->author);
                }
                else {
                    track->SetValue("artist", info->system);
                }

                if (strlen(info->song)) {
                    track->SetValue("title", info->song);
                }
                else {
                    track->SetValue("title", defaultTitle.c_str());
                }

                indexer->Save(source, track, externalId.c_str());

                track->Release();
            }
            gme_free_info(info);
        }
    }
    gme_delete(data);
}

static void scanDirectory(const std::string& path, IIndexerSource* source, IIndexerWriter* indexer) {
    DIR *dir = nullptr;
    struct dirent *entry = nullptr;

    if (!(dir = opendir(path.c_str()))) {
        return;
    }

    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") {
                continue;
            }
            scanDirectory(path + "/" + name, source, indexer);
        }
        else {
            std::string fn = entry->d_name;
            if (canHandle(fn)) {
                std::string fullFn = path + "/" + fn;
                updateMetadata(fullFn, source, indexer);
            }
        }
    }

    closedir(dir);
}

GmeIndexerSource::GmeIndexerSource() {
}

GmeIndexerSource::~GmeIndexerSource() {
}

void GmeIndexerSource::Release() {
    delete this;
}

void GmeIndexerSource::OnBeforeScan() {
}

void GmeIndexerSource::OnAfterScan() {
}

ScanResult GmeIndexerSource::Scan(
    IIndexerWriter* indexer,
    const char** indexerPaths,
    unsigned indexerPathsCount)
{
    for (size_t i = 0; i < indexerPathsCount; i++) {
        scanDirectory(std::string(indexerPaths[i]), this, indexer);
    }

    return ScanCommit;
}

void GmeIndexerSource::Interrupt() {

}

void GmeIndexerSource::ScanTrack(
    IIndexerWriter* indexer,
    ITagStore* tagStore,
    const char* externalId)
{
    if (!exists(externalId)) {
        indexer->RemoveByExternalId(this, externalId);
    }
}

int GmeIndexerSource::SourceId() {
    return std::hash<std::string>()(PLUGIN_NAME);
}