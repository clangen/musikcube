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
#include <common/String.h>
#include <common/Filesystem.h>
#include <string>
#include <algorithm>
#include <memory>
#include <exception>
#include <sstream>
#include <set>
#include <map>
#include <common/NarrowCast.h>
#include <gme/gme.h>

using namespace musik::core::sdk;


GmeIndexerSource::GmeIndexerSource(const mc_host_api& host, const mc_plugin_services& services)
: services(host, services) {}

const std::string& GmeIndexerSource::Key() {
    static const std::string key = PLUGIN_NAME;
    return key;
}



void GmeIndexerSource::OnBeforeScan() {
    this->filesIndexed = this->tracksIndexed = 0;
    this->interrupt = false;
    this->paths.clear();
}

void GmeIndexerSource::OnAfterScan() {
    invalidFiles.clear();
}

bool GmeIndexerSource::Scan(
    musik::core::plugin::IndexerWriter* indexer,
    const char** indexerPaths,
    size_t indexerPathsCount)
{
    /* keep these for later, for the removal phase */
    for (size_t i = 0; i < indexerPathsCount; i++) {
        this->paths.insert(fs::canonicalizePath(std::string(indexerPaths[i])));
    }

    std::exception_ptr failure;
    auto checkFile = [this, indexer, &failure](const std::string& path) {
        if (!failure && canHandle(path)) {
            try {
                this->UpdateMetadata(path, indexer);
            }
            catch (...) {
                failure = std::current_exception();
            }
        }
    };

    auto checkInterrupt = [this, &failure]() -> bool {
        return this->interrupt || failure != nullptr;
    };

    for (auto& path : this->paths) {
        if (!this->interrupt) {
            fs::scanDirectory(std::string(path), checkFile, checkInterrupt);
        }
    }

    if (failure) {
        std::rethrow_exception(failure);
    }

    indexer->CommitProgress(std::min<unsigned>(
        bounded_unsigned(this->filesIndexed), MC_INDEXER_PROGRESS_MAX));

    return true;
}

void GmeIndexerSource::Interrupt() {
    this->interrupt = true;
}

void GmeIndexerSource::ScanTrack(
    musik::core::plugin::IndexerWriter* indexer,
    const char* externalId)
{
    std::string fn;
    int trackNum;
    if (musik::core::plugin::ParseExternalId(EXTERNAL_ID_PREFIX, std::string(externalId), fn, trackNum)) {
        fn = fs::canonicalizePath(fn);

        /* if the file doesn't exist anymore, or it was flagged as invalid,
        we remove it */
        if (!fs::fileExists(fn) || invalidFiles.find(fn) != invalidFiles.end()) {
            indexer->RemoveByExternalId(externalId);
            return;
        }

        /* otherwise, we remove it if it doesn't exist in the list of paths
        we're supposed to be indexing */
        for (auto& path : this->paths) {
            if (fn.find(path) == 0) {
                return; /* found a match, we're good */
            }
        }

        indexer->RemoveByExternalId(externalId);
    }
}



void GmeIndexerSource::UpdateMetadata(
    std::string fn,
    musik::core::plugin::IndexerWriter* indexer)
{
    /* only need to do this check once, and it's relatively expensive because
    it requires a db read. cache we've already done it. */
    int64_t modifiedTime = fs::getLastModifiedTime(fn);
    const std::string firstExternalId = musik::core::plugin::CreateExternalId(EXTERNAL_ID_PREFIX, fn, 0);
    const auto modifiedDbTime = indexer->GetLastModifiedTime(firstExternalId);
    if (!modifiedDbTime || modifiedTime != *modifiedDbTime) {
        fn = fs::canonicalizePath(fn);

        Music_Emu* data = nullptr;
        gme_err_t err = gme_open_file(fn.c_str(), &data, gme_info_only);
        std::unique_ptr<Music_Emu, decltype(&gme_delete)> dataOwner(data, gme_delete);
        if (err) {
            this->services.Error("GME decoder", str::Format("error opening %s", fn.c_str()).c_str());
            invalidFiles.insert(fn);
        }
        else {
            double minTrackLength = this->services.Number(
                KEY_MINIMUM_TRACK_LENGTH, DEFAULT_MINIMUM_TRACK_LENGTH);

            bool ignoreSfx = this->services.Boolean(
                KEY_EXCLUDE_SOUND_EFFECTS, DEFAULT_EXCLUDE_SOUND_EFFECTS);

            if (this->services.Boolean(KEY_ENABLE_M3U, DEFAULT_ENABLE_M3U)) {
                std::string m3u = getM3uFor(fn);
                if (m3u.size()) {
                    err = gme_load_m3u(data, m3u.c_str());
                    if (err) {
                        this->services.Error("GME decoder", str::Format("m3u found, but load failed '%s'", err).c_str());
                    }
                }
            }

            const std::string defaultDuration =
                std::to_string(this->services.Number(
                    KEY_DEFAULT_TRACK_LENGTH,
                    DEFAULT_TRACK_LENGTH));

            const std::string directory = fs::getDirectory<std::string>(fn);

            for (int i = 0; i < gme_track_count(data); i++) {
                const std::string externalId = musik::core::plugin::CreateExternalId(EXTERNAL_ID_PREFIX, fn, i);
                const std::string trackNum = std::to_string(i + 1);
                const std::string defaultTitle = "Track " + std::to_string(1 + i);
                const std::string modifiedTimeStr = std::to_string(modifiedTime);

                musik::core::plugin::IndexerRecord track(indexer->Get());
                track.SetValue("filename", externalId.c_str());
                track.SetValue("directory", directory.c_str());
                track.SetValue("filetime", modifiedTimeStr.c_str());
                track.SetValue("track", trackNum.c_str());

                gme_info_t* info = nullptr;
                err = gme_track_info(data, &info, i);
                std::unique_ptr<gme_info_t, decltype(&gme_free_info)> infoOwner(info, gme_free_info);
                if (err) {
                    this->services.Error("GME decoder", str::Format("error getting track %d: %s", i, err).c_str());
                    track.SetValue("duration", defaultDuration.c_str());
                    track.SetValue("title", defaultTitle.c_str());
                }
                else if (info) {
                    /* don't index tracks that are shorter than the specified minimum length.
                    this allows users to ignore things like sound effects */
                    if (minTrackLength > 0.0 &&
                        ignoreSfx &&
                        info->length > 0 &&
                        info->length / 1000.0 < minTrackLength)
                    {
                        continue;
                    }

                    std::string duration = (info->length == -1)
                        ? defaultDuration
                        : std::to_string((float) info->play_length / 1000.0f);

                    track.SetValue("album", info->game);
                    track.SetValue("album_artist", info->system);
                    track.SetValue("genre", info->system);
                    track.SetValue("duration", duration.c_str());
                    track.SetValue("artist", strlen(info->author) ? info->author : info->system);
                    track.SetValue("title", strlen(info->song) ? info->song : defaultTitle.c_str());
                }

                indexer->Save(track, externalId);
                ++tracksIndexed;
            }
        }

    }

    /* we commit progress every so often */
    if (++this->filesIndexed % 300 == 0) {
        indexer->CommitProgress(std::min<unsigned>(
            bounded_unsigned_sum(this->filesIndexed, this->tracksIndexed), MC_INDEXER_PROGRESS_MAX));
        this->filesIndexed = this->tracksIndexed = 0;
    }
}
