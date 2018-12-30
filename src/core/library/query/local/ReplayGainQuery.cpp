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

#include "pch.hpp"
#include "ReplayGainQuery.h"

using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::sdk;

ReplayGainQuery::ReplayGainQuery(int64_t trackId) {
    this->trackId = trackId;
}

ReplayGainQuery::~ReplayGainQuery() {

}

ReplayGainQuery::Result ReplayGainQuery::GetResult() {
    return this->result;
}

bool ReplayGainQuery::OnRun(musik::core::db::Connection &db) {
    Statement stmt(
        "SELECT album_gain, album_peak, track_gain, track_peak "
        "FROM replay_gain "
        "WHERE track_id=?",
        db);

    stmt.BindInt64(0, this->trackId);

    this->result = std::make_shared<ReplayGain>();
    this->result->albumGain = this->result->albumPeak = 1.0f;
    this->result->trackGain = this->result->trackPeak = 1.0f;

    if (stmt.Step() == db::Row) {
        this->result->albumGain = stmt.ColumnFloat(0);
        this->result->albumPeak = stmt.ColumnFloat(1);
        this->result->trackGain = stmt.ColumnFloat(2);
        this->result->trackPeak = stmt.ColumnFloat(3);
    }

    return true;
}