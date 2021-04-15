//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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
#include "NowPlayingTrackListQuery.h"

#include <musikcore/library/track/LibraryTrack.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/db/Statement.h>

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using musik::core::ILibraryPtr;

using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::core::library::query;

const std::string NowPlayingTrackListQuery::kQueryName = "NowPlayingTrackListQuery";

NowPlayingTrackListQuery::NowPlayingTrackListQuery(
    ILibraryPtr library, musik::core::audio::PlaybackService& playback)
: library(library)
, playback(playback) {
    this->result = std::make_shared<TrackList>(library);
    this->headers = std::make_shared<std::set<size_t>>();
    this->hash = 0;
}

NowPlayingTrackListQuery::Result NowPlayingTrackListQuery::GetResult() noexcept {
    return this->result;
}

NowPlayingTrackListQuery::Headers NowPlayingTrackListQuery::GetHeaders() noexcept {
    return this->headers;
}

size_t NowPlayingTrackListQuery::GetQueryHash() noexcept {
    if (this->hash == 0) {
        this->hash = std::hash<std::string>()(this->Name());
    }

    return this->hash;
}

bool NowPlayingTrackListQuery::OnRun(Connection& db) {
    if (result) {
        this->result = std::make_shared<TrackList>(library);
        this->headers = std::make_shared<std::set<size_t>>();
    }

    this->playback.CopyTo(*result);

    return true;
}
