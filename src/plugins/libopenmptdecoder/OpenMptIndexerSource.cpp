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

#include "Utility.h"
#include "OpenMptIndexerSource.h"
#include <common/Filesystem.h>
#include <string>
#include <algorithm>
#include <memory>
#include <exception>
#include <sstream>
#include <set>
#include <map>
#include <common/NarrowCast.h>

#pragma warning(push, 0)
#include <libopenmpt/libopenmpt.h>
#pragma warning(pop)

using namespace musik::core::sdk;


static void openmptLogFunc(const char* message, void* userdata) {
    static_cast<OpenMptIndexerSource*>(userdata)->Log(message);
}

OpenMptIndexerSource::OpenMptIndexerSource(const mc_host_api& host, const mc_plugin_services& services)
: services(host, services) {}

const std::string& OpenMptIndexerSource::Key() {
    return PLUGIN_NAME;
}



void OpenMptIndexerSource::Log(const char* message) {
    this->services.Info("OpenMptIndexerSource", message);
}

void OpenMptIndexerSource::OnBeforeScan() {
    this->filesIndexed = this->tracksIndexed = 0;
    this->interrupt = false;
    this->paths.clear();
}

void OpenMptIndexerSource::OnAfterScan() {
    invalidFiles.clear();
}

bool OpenMptIndexerSource::Scan(
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
        if (!failure && isFileSupported(path)) {
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

void OpenMptIndexerSource::Interrupt() {
    this->interrupt = true;
}

void OpenMptIndexerSource::ScanTrack(
    musik::core::plugin::IndexerWriter* indexer,
    const char* externalId)
{
    std::string fn;
    int trackNum;
    if (musik::core::plugin::ParseExternalId(PLUGIN_NAME, std::string(externalId), fn, trackNum)) {
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



static std::string formatDefaultValue(const musik::core::plugin::IndexerServices& services,
    const char* key, const char* defaultValue, std::string type)
{
    type = type.empty() ? "mod" : type;
    const auto pattern = services.Text(key, defaultValue);
    /* user-entered text may contain printf directives that are unsafe to execute. */
    std::string value;
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '%' && i + 1 < pattern.size() && pattern[i + 1] == 's') {
            value += type;
            ++i;
        }
        else if (pattern[i] == '%' && i + 1 < pattern.size() && pattern[i + 1] == '%') {
            value += '%';
            ++i;
        }
        else {
            value += pattern[i];
        }
    }
    return value;
}

void OpenMptIndexerSource::UpdateMetadata(
    std::string fn,
    musik::core::plugin::IndexerWriter* indexer)
{
    /* only need to do this check once, and it's relatively expensive because
    it requires a db read. cache we've already done it. */
    int64_t modifiedTime = fs::getLastModifiedTime(fn);
    const std::string firstExternalId = musik::core::plugin::CreateExternalId(PLUGIN_NAME, fn, 0);
    const auto modifiedDbTime = indexer->GetLastModifiedTime(firstExternalId);
    if (!modifiedDbTime || modifiedTime != *modifiedDbTime) {
        fn = fs::canonicalizePath(fn);
        char* fileBytes = nullptr;
        int fileBytesSize = 0;
        if (fileToByteArray(fn, &fileBytes, fileBytesSize)) {
            std::unique_ptr<char, decltype(&free)> fileOwner(fileBytes, free);
            openmpt_module* module = openmpt_module_create_from_memory2(
                fileBytes, (size_t)fileBytesSize, openmptLogFunc, this,
                nullptr, nullptr, nullptr, nullptr, nullptr);
            std::unique_ptr<openmpt_module, decltype(&openmpt_module_destroy)> moduleOwner(module, openmpt_module_destroy);

            if (!module) {
                this->services.Error(PLUGIN_NAME.c_str(), str::Format("error opening %s", fn.c_str()).c_str());
                invalidFiles.insert(fn);
            }
            else {
                std::string directory = fs::getDirectory(fn);
                std::string extension = fs::getFileExtension(fn);
                const int32_t count = openmpt_module_get_num_subsongs(module);
                if (count > 0) {
                    for (int32_t i = 0; i < count; i++) {
                        openmpt_module_select_subsong(module, i);

                        const std::string externalId = musik::core::plugin::CreateExternalId(PLUGIN_NAME, fn, i);
                        const std::string trackNum = std::to_string(i + 1);
                        const std::string modifiedTimeStr = std::to_string(modifiedTime);

                        std::string album = readMetadataValue(module, "container_long");
                        if (!album.size()) {
                            album = readMetadataValue(module, "container").c_str();
                            if (!album.size()) {
                                album = formatDefaultValue(this->services,
                                    KEY_DEFAULT_ALBUM_NAME, DEFAULT_ALBUM_NAME, extension);
                            }
                        }

                        std::string title = readMetadataValue(module, "title");
                        if (!title.size()) {
                            title = "Track " + std::to_string(1 + i);
                        }

                        std::string artist = readMetadataValue(module, "artist");
                        if (!artist.size()) {
                            artist = formatDefaultValue(this->services,
                                KEY_DEFAULT_ARTIST_NAME, DEFAULT_ARTIST_NAME, extension);
                        }

                        const std::string duration = std::to_string(
                            openmpt_module_get_duration_seconds(module));

                        musik::core::plugin::IndexerRecord track(indexer->Get());
                        track.SetValue("filename", externalId.c_str());
                        track.SetValue("directory", directory.c_str());
                        track.SetValue("filetime", modifiedTimeStr.c_str());
                        track.SetValue("track", trackNum.c_str());
                        track.SetValue("album", album.c_str());
                        track.SetValue("title", title.c_str());
                        track.SetValue("genre", "Electronic");
                        track.SetValue("artist", artist.c_str());
                        track.SetValue("album_artist", artist.c_str());
                        track.SetValue("duration", duration.c_str());

                        indexer->Save(track, externalId);
                        ++tracksIndexed;
                    }
                }


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
