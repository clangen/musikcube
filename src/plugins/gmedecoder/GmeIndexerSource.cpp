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

#include "Constants.h"
#include "GmeIndexerSource.h"
#include <musikcore/sdk/IDebug.h>
#include <musikcore/sdk/IPreferences.h>
#include <musikcore/sdk/String.h>
#include <musikcore/sdk/Filesystem.h>
#include <string>
#include <sstream>
#include <set>
#include <map>
#include <gme/gme.h>

using namespace musik::core::sdk;

extern IDebug* debug;
extern IPreferences* prefs;

GmeIndexerSource::GmeIndexerSource() {
}

GmeIndexerSource::~GmeIndexerSource() {
}

void GmeIndexerSource::Release() {
    delete this;
}

void GmeIndexerSource::OnBeforeScan() {
    this->filesIndexed = this->tracksIndexed = 0;
    this->interrupt = false;
    this->paths.clear();
}

void GmeIndexerSource::OnAfterScan() {
    invalidFiles.clear();
}

ScanResult GmeIndexerSource::Scan(
    IIndexerWriter* indexer,
    const char** indexerPaths,
    unsigned indexerPathsCount)
{
    /* keep these for later, for the removal phase */
    for (size_t i = 0; i < indexerPathsCount; i++) {
        this->paths.insert(fs::canonicalizePath(std::string(indexerPaths[i])));
    }

    auto checkFile = [this, indexer](const std::string& path) {
        if (canHandle(path)) {
            try {
                this->UpdateMetadata(path, this, indexer);
            }
            catch (...) {
                std::string error = str::Format("error reading metadata for %s", path.c_str());
                debug->Error(PLUGIN_NAME, error.c_str());
            }
        }
    };

    auto checkInterrupt = [this]() -> bool {
        return this->interrupt;
    };

    for (auto& path : this->paths) {
        if (!this->interrupt) {
            fs::scanDirectory(std::string(path), checkFile, checkInterrupt);
        }
    }

    indexer->CommitProgress(this, this->filesIndexed);

    return ScanCommit;
}

void GmeIndexerSource::Interrupt() {
    this->interrupt = true;
}

void GmeIndexerSource::ScanTrack(
    IIndexerWriter* indexer,
    ITagStore* tagStore,
    const char* externalId)
{
    std::string fn;
    int trackNum;
    if (indexer::parseExternalId(EXTERNAL_ID_PREFIX, std::string(externalId), fn, trackNum)) {
        fn = fs::canonicalizePath(fn);

        /* if the file doesn't exist anymore, or it was flagged as invalid,
        we remove it */
        if (!fs::fileExists(fn) || invalidFiles.find(fn) != invalidFiles.end()) {
            indexer->RemoveByExternalId(this, externalId);
            return;
        }

        /* otherwise, we remove it if it doesn't exist in the list of paths
        we're supposed to be indexing */
        for (auto& path : this->paths) {
            if (fn.find(path) == 0) {
                return; /* found a match, we're good */
            }
        }

        indexer->RemoveByExternalId(this, externalId);
    }
}

int GmeIndexerSource::SourceId() {
    return std::hash<std::string>()(PLUGIN_NAME);
}

void GmeIndexerSource::UpdateMetadata(
    std::string fn,
    IIndexerSource* source,
    IIndexerWriter* indexer)
{
    /* only need to do this check once, and it's relatively expensive because
    it requires a db read. cache we've already done it. */
    int64_t modifiedTime = fs::getLastModifiedTime(fn);
    const std::string firstExternalId = indexer::createExternalId(EXTERNAL_ID_PREFIX, fn, 0);
    int64_t modifiedDbTime = indexer->GetLastModifiedTime(this, firstExternalId.c_str());
    if (modifiedDbTime < 0 || modifiedTime != modifiedDbTime) {
        fn = fs::canonicalizePath(fn);

        Music_Emu* data = nullptr;
        gme_err_t err = gme_open_file(fn.c_str(), &data, gme_info_only);
        if (err) {
            debug->Error(PLUGIN_NAME, str::Format("error opening %s", fn.c_str()).c_str());
            invalidFiles.insert(fn);
        }
        else {
            double minTrackLength = prefs->GetDouble(
                KEY_MINIMUM_TRACK_LENGTH, DEFAULT_MINIMUM_TRACK_LENGTH);

            bool ignoreSfx = prefs->GetBool(
                KEY_EXCLUDE_SOUND_EFFECTS, DEFAULT_EXCLUDE_SOUND_EFFECTS);

            if (prefs->GetBool(KEY_ENABLE_M3U, DEFAULT_ENABLE_M3U)) {
                std::string m3u = getM3uFor(fn);
                if (m3u.size()) {
                    err = gme_load_m3u(data, m3u.c_str());
                    if (err) {
                        debug->Error(PLUGIN_NAME, str::Format("m3u found, but load failed '%s'", err).c_str());
                    }
                }
            }

            const std::string defaultDuration =
                std::to_string(prefs->GetDouble(
                    KEY_DEFAULT_TRACK_LENGTH,
                    DEFAULT_TRACK_LENGTH));

            const std::string directory = fs::getDirectory<std::string>(fn);

            for (int i = 0; i < gme_track_count(data); i++) {
                const std::string externalId = indexer::createExternalId(EXTERNAL_ID_PREFIX, fn, i);
                const std::string trackNum = std::to_string(i + 1);
                const std::string defaultTitle = "Track " + std::to_string(1 + i);
                const std::string modifiedTimeStr = std::to_string(modifiedTime);

                auto track = indexer->CreateWriter();
                track->SetValue("filename", externalId.c_str());
                track->SetValue("directory", directory.c_str());
                track->SetValue("filetime", modifiedTimeStr.c_str());
                track->SetValue("track", trackNum.c_str());

                gme_info_t* info = nullptr;
                err = gme_track_info(data, &info, i);
                if (err) {
                    debug->Error(PLUGIN_NAME, str::Format("error getting track %d: %s", i, err).c_str());
                    track->SetValue("duration", defaultDuration.c_str());
                    track->SetValue("title", defaultTitle.c_str());
                }
                else if (info) {
                    /* don't index tracks that are shorter than the specified minimum length.
                    this allows users to ignore things like sound effects */
                    if (minTrackLength > 0.0 &&
                        ignoreSfx &&
                        info->length > 0 &&
                        info->length / 1000.0 < minTrackLength)
                    {
                        gme_free_info(info);
                        continue;
                    }

                    std::string duration = (info->length == -1)
                        ? defaultDuration
                        : std::to_string((float) info->play_length / 1000.0f);

                    track->SetValue("album", info->game);
                    track->SetValue("album_artist", info->system);
                    track->SetValue("genre", info->system);
                    track->SetValue("duration", duration.c_str());
                    track->SetValue("artist", strlen(info->author) ? info->author : info->system);
                    track->SetValue("title", strlen(info->song) ? info->song : defaultTitle.c_str());
                }

                gme_free_info(info);
                indexer->Save(source, track, externalId.c_str());
                track->Release();
                ++tracksIndexed;
            }
        }

        gme_delete(data);
    }

    /* we commit progress every so often */
    if (++this->filesIndexed % 300 == 0) {
        indexer->CommitProgress(this, this->filesIndexed + this->tracksIndexed);
        this->filesIndexed = this->tracksIndexed = 0;
    }
}