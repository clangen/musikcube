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
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <musikcore/plugin/indexer.hpp>
#include <atomic>
#include <chrono>
#include <set>
#include <string>

class GmeIndexerSource {
public:
    GmeIndexerSource(const mc_host_api& host, const mc_plugin_services& services);

    static const std::string& Key();
    static const std::string& Label();

    void OnBeforeScan();
    void OnAfterScan();
    bool Scan(musik::core::plugin::IndexerWriter* writer, const char** paths, size_t count);
    void ScanTrack(musik::core::plugin::IndexerWriter* writer, const char* externalId);
    void Interrupt();
    bool HasStableIds() const { return true; }
    bool NeedsTrackScan() const { return true; }

private:
    void UpdateMetadata(std::string path, musik::core::plugin::IndexerWriter* writer);

    musik::core::plugin::IndexerServices services;
    std::set<std::string> invalidFiles;
    std::set<std::string> paths;
    std::chrono::steady_clock::time_point lastCheckpoint;
    size_t filesIndexed { 0 };
    size_t tracksIndexed { 0 };
    std::atomic<bool> interrupt { false };
};
