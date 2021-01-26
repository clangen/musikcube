//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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
#include <musikcore/sdk/IDebug.h>
#include <musikcore/sdk/IPreferences.h>
#include <musikcore/sdk/Filesystem.h>
#include <string>
#include <sstream>
#include <set>
#include <map>

#pragma warning(push, 0)
#include <libopenmpt/libopenmpt.h>
#pragma warning(pop)

using namespace musik::core::sdk;

extern IDebug* debug;
extern IPreferences* prefs;

static void openmptLogFunc(const char *message, void *userdata) {
    if (debug) {
        debug->Info("OpenMptIndexerSource", message);
    }
}

OpenMptIndexerSource::OpenMptIndexerSource() {
}

OpenMptIndexerSource::~OpenMptIndexerSource() {
}

void OpenMptIndexerSource::Release() {
    delete this;
}

void OpenMptIndexerSource::OnBeforeScan() {
    this->filesIndexed = this->tracksIndexed = 0;
    this->interrupt = false;
    this->paths.clear();
}

void OpenMptIndexerSource::OnAfterScan() {
    invalidFiles.clear();
}

ScanResult OpenMptIndexerSource::Scan(
    IIndexerWriter* indexer,
    const char** indexerPaths,
    unsigned indexerPathsCount)
{
    /* keep these for later, for the removal phase */
    for (size_t i = 0; i < indexerPathsCount; i++) {
        this->paths.insert(fs::canonicalizePath(std::string(indexerPaths[i])));
    }

    auto checkFile = [this, indexer](const std::string& path) {
        if (isFileSupported(path)) {
            try {
                this->UpdateMetadata(path, this, indexer);
            }
            catch (...) {
                std::string error = str::format("error reading metadata for %s", path.c_str());
                debug->Error(PLUGIN_NAME.c_str(), error.c_str());
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

void OpenMptIndexerSource::Interrupt() {
    this->interrupt = true;
}

void OpenMptIndexerSource::ScanTrack(
    IIndexerWriter* indexer,
    ITagStore* tagStore,
    const char* externalId)
{
    std::string fn;
    int trackNum;
    if (indexer::parseExternalId(PLUGIN_NAME, std::string(externalId), fn, trackNum)) {
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

int OpenMptIndexerSource::SourceId() {
    return std::hash<std::string>()(PLUGIN_NAME);
}

static std::string formatDefaultValue(const char* key, const char* defaultValue, std::string type) {
#ifdef __APPLE__
    static __thread char threadLocalBuffer[4096];
#else
    static thread_local char threadLocalBuffer[4096];
#endif
    type = type.empty() ? "mod" : type;
    prefs->GetString(key, threadLocalBuffer, 4096, defaultValue);
    std::string value(threadLocalBuffer);
    threadLocalBuffer[0] = 0;
    return str::format(value, type.c_str());
}

void OpenMptIndexerSource::UpdateMetadata(
    std::string fn,
    IIndexerSource* source,
    IIndexerWriter* indexer)
{
    /* only need to do this check once, and it's relatively expensive because
    it requires a db read. cache we've already done it. */
    int modifiedTime = fs::getLastModifiedTime(fn);
    const std::string firstExternalId = indexer::createExternalId(PLUGIN_NAME, fn, 0);
    int modifiedDbTime = indexer->GetLastModifiedTime(this, firstExternalId.c_str());
    if (modifiedDbTime < 0 || modifiedTime != modifiedDbTime) {
        fn = fs::canonicalizePath(fn);
        char* fileBytes = nullptr;
        int fileBytesSize = 0;
        if (fileToByteArray(fn, &fileBytes, fileBytesSize)) {
            openmpt_module* module = openmpt_module_create_from_memory2(
                fileBytes, (size_t)fileBytesSize, openmptLogFunc, this,
                nullptr, nullptr, nullptr, nullptr, nullptr);

            if (!module) {
                debug->Error(PLUGIN_NAME.c_str(), str::format("error opening %s", fn.c_str()).c_str());
                invalidFiles.insert(fn);
            }
            else {
                std::string directory = fs::getDirectory(fn);
                std::string extension = fs::getFileExtension(fn);
                size_t count = openmpt_module_get_num_subsongs(module);
                const char* keys = openmpt_module_get_metadata_keys(module);
                if (count > 0) {
                    for (size_t i = 0; i < count; i++) {
                        openmpt_module_select_subsong(module, i);

                        const std::string externalId = indexer::createExternalId(PLUGIN_NAME, fn, i);
                        const std::string trackNum = std::to_string(i + 1);
                        const std::string modifiedTimeStr = std::to_string(modifiedTime);

                        std::string album = readMetadataValue(module, "container_long");
                        if (!album.size()) {
                            album = readMetadataValue(module, "container").c_str();
                            if (!album.size()) {
                                album = formatDefaultValue(
                                    KEY_DEFAULT_ALBUM_NAME, DEFAULT_ALBUM_NAME, extension);
                            }
                        }

                        std::string title = readMetadataValue(module, "title");
                        if (!title.size()) {
                            title = "Track " + std::to_string(1 + i);
                        }

                        std::string artist = readMetadataValue(module, "artist");
                        if (!artist.size()) {
                            artist = formatDefaultValue(
                                KEY_DEFAULT_ARTIST_NAME, DEFAULT_ARTIST_NAME, extension);
                        }

                        const std::string duration = std::to_string(
                            openmpt_module_get_duration_seconds(module));

                        auto track = indexer->CreateWriter();
                        track->SetValue("filename", externalId.c_str());
                        track->SetValue("directory", directory.c_str());
                        track->SetValue("filetime", modifiedTimeStr.c_str());
                        track->SetValue("track", trackNum.c_str());
                        track->SetValue("album", album.c_str());
                        track->SetValue("title", title.c_str());
                        track->SetValue("genre", "Electronic");
                        track->SetValue("artist", artist.c_str());
                        track->SetValue("album_artist", artist.c_str());
                        track->SetValue("duration", duration.c_str());

                        indexer->Save(source, track, externalId.c_str());
                        track->Release();
                        ++tracksIndexed;
                    }
                }

                openmpt_module_destroy(module);
                module = nullptr;
            }

            free(fileBytes);
        }
    }

    /* we commit progress every so often */
    if (++this->filesIndexed % 300 == 0) {
        indexer->CommitProgress(this, this->filesIndexed + this->tracksIndexed);
        this->filesIndexed = this->tracksIndexed = 0;
    }
}
