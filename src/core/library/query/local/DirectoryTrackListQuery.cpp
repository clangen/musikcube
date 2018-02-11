//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include "pch.hpp"
#include <core/library/LocalLibraryConstants.h>
#include "DirectoryTrackListQuery.h"
#include "CategoryTrackListQuery.h"

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::ILibraryPtr;

using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::library;

DirectoryTrackListQuery::DirectoryTrackListQuery(
    ILibraryPtr library,
    const std::string& directory,
    const std::string& filter)
{
    this->library = library;
    this->directory = directory;
    this->filter = filter;
    this->result.reset(new musik::core::TrackList(library));
    this->headers.reset(new std::set<size_t>());
    this->hash = std::hash<std::string>()(directory + "-" + filter);
}

DirectoryTrackListQuery::~DirectoryTrackListQuery() {

}

bool DirectoryTrackListQuery::OnRun(Connection& db) {
    Statement query("SELECT id FROM directories WHERE name=?", db);
    query.BindText(0, this->directory);

    if (query.Step() == db::Row) {
        int64_t id = query.ColumnInt64(0);

        auto tracklistQuery = std::make_shared<CategoryTrackListQuery>(
            this->library, constants::Track::DIRECTORY, id, this->filter);

        if (this->library->Enqueue(tracklistQuery, ILibrary::QuerySynchronous)) {
            this->result = tracklistQuery->GetResult();
            this->headers = tracklistQuery->GetHeaders();
        }
    }

    return true;
}
