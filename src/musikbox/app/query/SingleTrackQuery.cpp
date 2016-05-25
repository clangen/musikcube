#pragma once

#include "stdafx.h"
#include "SingleTrackQuery.h"

#include <core/library/track/LibraryTrack.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/db/Statement.h>

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;

using namespace musik::core::library::constants;
using namespace musik::box;

SingleTrackQuery::SingleTrackQuery(const std::string& filename) {
    this->filename = filename;
}

SingleTrackQuery::~SingleTrackQuery() {

}

TrackPtr SingleTrackQuery::GetResult() {
    return this->result;
}

bool SingleTrackQuery::OnRun(Connection& db) {
    result.reset(new LibraryTrack());
    result->SetValue("filename", filename.c_str());
    return LibraryTrack::Load(result.get(), db);
}