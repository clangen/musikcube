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

using namespace musik::core::sdk;

static std::vector<std::string> tokenize(const std::string& str, char delim = '/') {
    std::vector<std::string> result;
    std::istringstream iss(str);
    std::string token;

    while (std::getline(iss, token, delim)) {
        result.push_back(token);
    }

    return result;
}

static std::string createExternalId(const std::string& fn, int track) {
    return "gme/" + std::to_string(track) + "/" + fn;
}

static bool exists(const std::string& externalId) {
    return false;
}

static void updateMetadata(const std::string& fn, IIndexerWriter* indexer) {
    // gme_t* data = nullptr;
    // gme_err_t err = gme_open_file("/tmp/mario.nsf", &data, 44100);
    // if (err) {
    //     std::cerr << "error: " << err << "\n";
    // }
    // else {
    //     gme_info_t* info = nullptr;
    //     err = gme_track_info(data, &info, 0);
    //     if (err) {
    //         std::cerr << "error: " << err << "\n";
    //     }
    //     gme_free_info(info);
    // }
    // gme_delete(data);
}

static void scanDirectory(const std::string& path, IIndexerWriter* indexer) {

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
        scanDirectory(std::string(indexerPaths[i]), indexer);
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
    // if (!exists(this->discIds, this->model, externalId)) {
    //     indexer->RemoveByExternalId(this, externalId);
    // }
}

int GmeIndexerSource::SourceId() {
    return std::hash<std::string>()(PLUGIN_NAME);
}