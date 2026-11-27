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
#include "GmeFile.h"
#include <common/String.h>
#include <common/Filesystem.h>
#include <common/Utf8.h>
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

constexpr size_t kMaxBatchFiles = 300;
constexpr auto kMaxBatchDuration = std::chrono::milliseconds(250);


GmeIndexerSource::GmeIndexerSource(const mc_host_api& host, const mc_plugin_services& services)
: services(host, services) {}

const std::string& GmeIndexerSource::Label() {
    static const std::string label = "Game music";
    return label;
}

const std::string& GmeIndexerSource::Key() {
    static const std::string key = PLUGIN_NAME;
    return key;
}



void GmeIndexerSource::OnBeforeScan() {
    this->filesIndexed = this->tracksIndexed = 0;
    this->lastCheckpoint = std::chrono::steady_clock::now();
    this->interrupt = false;
    this->paths.clear();
    this->files.clear();
}

void GmeIndexerSource::OnAfterScan() {
    invalidFiles.clear();
    this->files.clear();
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
            catch (const std::exception& error) {
                this->services.Error("GME decoder", "could not index " + path + ": " + u8sanitize(error.what()));
                failure = std::current_exception();
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
    const char* externalId,
    int64_t lastModified)
{
    std::string fn;
    int trackNum;
    if (musik::core::plugin::ParseExternalId(EXTERNAL_ID_PREFIX, std::string(externalId), fn, trackNum)) {
        auto file = this->files.find(fn);
        if (file == this->files.end()) {
            fn = fs::canonicalizePath(fn);
            file = this->files.find(fn);
        }

        bool exists;
        if (file != this->files.end()) {
            if (file->second.exists == -1) {
                file->second.exists = fs::fileExists(fn) ? 1 : 0;
            }
            exists = file->second.exists == 1;
        }
        else {
            exists = fs::fileExists(fn);
        }

        /* if the file doesn't exist anymore, or it was flagged as invalid,
        we remove it */
        if (!exists || invalidFiles.find(fn) != invalidFiles.end()) {
            indexer->RemoveByExternalId(externalId);
            return;
        }

        /* otherwise, we remove it if it doesn't exist in the list of paths
        we're supposed to be indexing */
        bool withinPath = false;
        for (const auto& path : this->paths) {
            if (IsWithinDirectory(fn, path)) {
                withinPath = true;
                break;
            }
        }
        if (!withinPath) {
            indexer->RemoveByExternalId(externalId);
            return;
        }

        if (file == this->files.end() || file->second.lastModified == lastModified) {
            return;
        }
        /* an older row may belong to a container that lost tracks. inspect
        the current file once, retaining rows if the file cannot be read. */
        if (file->second.trackCount == -1) {
            file->second.trackCount = this->ReadTrackCount(fn);
        }
        if (file->second.trackCount >= 0 && trackNum >= file->second.trackCount) {
            try {
                if (fs::getLastModifiedTime(fn) == file->second.lastModified) {
                    indexer->RemoveByExternalId(externalId);
                }
            }
            catch (const std::filesystem::filesystem_error&) {
                /* a file changed or disappeared after the source scan. */
            }
        }
    }
}

bool GmeIndexerSource::LoadPlaylist(Music_Emu* data, const std::string& path) {
    if (!this->services.Boolean(KEY_ENABLE_M3U, DEFAULT_ENABLE_M3U)) {
        return true;
    }
    const auto m3u = getM3uFor(path);
    if (m3u.empty()) {
        return true;
    }
    std::vector<char> playlist;
    if (!ReadGmeFile(m3u, playlist)) {
        this->services.Error("GME decoder", "could not read playlist: " + m3u);
        return false;
    }
    else if (const auto error = gme_load_m3u_data(data, playlist.data(), static_cast<long>(playlist.size()))) {
        this->services.Error("GME decoder", str::Format("m3u found, but load failed '%s'", error));
        return false;
    }
    return true;
}

int GmeIndexerSource::ReadTrackCount(const std::string& path) {
    std::vector<char> bytes;
    Music_Emu* raw = nullptr;
    if (!ReadGmeFile(path, bytes)) {
        return -2;
    }
    const auto error = gme_open_data(bytes.data(), static_cast<long>(bytes.size()), &raw, gme_info_only);
    std::unique_ptr<Music_Emu, decltype(&gme_delete)> data(raw, gme_delete);
    if (error || !data) {
        return -2;
    }
    if (!this->LoadPlaylist(data.get(), path)) {
        return -2;
    }
    return gme_track_count(data.get());
}

void GmeIndexerSource::UpdateMetadata(
    std::string fn,
    musik::core::plugin::IndexerWriter* indexer)
{
    /* only need to do this check once, and it's relatively expensive because
    it requires a db read. cache we've already done it. */
    fn = fs::canonicalizePath(fn);
    int64_t modifiedTime;
    try {
        modifiedTime = fs::getLastModifiedTime(fn);
    }
    catch (const std::filesystem::filesystem_error&) {
        /* a file can disappear or become unreadable while its directory is scanned. */
        this->services.Error("GME decoder", "could not read file timestamp: " + fn);
        return;
    }
    auto& file = this->files[fn];
    file.lastModified = modifiedTime;
    file.trackCount = -1;
    file.exists = -1;
    const std::string firstExternalId = musik::core::plugin::CreateExternalId(EXTERNAL_ID_PREFIX, fn, 0);
    const auto modifiedDbTime = indexer->GetLastModifiedTime(firstExternalId);
    if (!modifiedDbTime || modifiedTime != *modifiedDbTime) {
        std::vector<char> fileBytes;
        if (!ReadGmeFile(fn, fileBytes)) {
            this->services.Error("GME decoder", "could not read file: " + fn);
            return;
        }
        Music_Emu* data = nullptr;
        gme_err_t err = gme_open_data(fileBytes.data(), static_cast<long>(fileBytes.size()), &data, gme_info_only);
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

            if (!this->LoadPlaylist(data, fn)) {
                return;
            }
            file.trackCount = gme_track_count(data);

            const std::string defaultDuration =
                std::to_string(this->services.Number(
                    KEY_DEFAULT_TRACK_LENGTH,
                    DEFAULT_TRACK_LENGTH));

            const std::string directory = fs::getDirectory<std::string>(fn);

            for (int i = 0; i < file.trackCount && !this->interrupt; i++) {
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
                        indexer->RemoveByExternalId(externalId.c_str());
                        continue;
                    }

                    std::string duration = (info->length == -1)
                        ? defaultDuration
                        : std::to_string((float) info->play_length / 1000.0f);

                    /* legacy GME metadata may contain bytes that are not UTF-8. */
                    const auto game = u8sanitize(info->game ? info->game : "");
                    const auto system = u8sanitize(info->system ? info->system : "");
                    const auto artist = u8sanitize(info->author && *info->author ? info->author : system);
                    const auto title = u8sanitize(info->song && *info->song ? info->song : defaultTitle);
                    track.SetValue("album", game.c_str());
                    track.SetValue("album_artist", system.c_str());
                    track.SetValue("genre", system.c_str());
                    track.SetValue("duration", duration.c_str());
                    track.SetValue("artist", artist.c_str());
                    track.SetValue("title", title.c_str());
                }

                indexer->Save(track, externalId);
                ++tracksIndexed;
            }
        }

    }

    /* keep slow reads from extending a batch to the file-count limit. */
    const auto now = std::chrono::steady_clock::now();
    if (++this->filesIndexed >= kMaxBatchFiles || now - this->lastCheckpoint >= kMaxBatchDuration) {
        indexer->CommitProgress(std::min<unsigned>(
            bounded_unsigned_sum(this->filesIndexed, this->tracksIndexed), MC_INDEXER_PROGRESS_MAX));
        this->filesIndexed = this->tracksIndexed = 0;
        this->lastCheckpoint = now;
    }
}
