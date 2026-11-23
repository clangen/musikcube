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

#pragma once

#include <atomic>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

/* one cache index is shared by the plugin's streams. each download writes a
private temporary file; readers of a completed track see only the final file. */
class LruDiskCache {
public:
    bool Initialize(const std::filesystem::path& directory, size_t maxEntries, const std::atomic<bool>& cancelled);
    FILE* OpenCached(size_t id, std::string& type, uint64_t& length);
    bool CreatePartial(std::filesystem::path& path, FILE*& writer, FILE*& reader);
    void Finish(size_t id, const std::filesystem::path& path, const std::string& type);

private:
    struct Entry {
        size_t id;
        std::filesystem::path path;
        std::string type;
        std::filesystem::file_time_type used;
    };

    void Prune();

    std::mutex mutex;
    std::filesystem::path root;
    size_t limit = 0;
    std::vector<Entry> entries;
};
