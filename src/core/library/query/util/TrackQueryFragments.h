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

#pragma once

#include <core/library/LocalLibraryConstants.h>
#include <core/library/track/Track.h>
#include <core/db/Statement.h>
#include <core/db/Connection.h>
#include <core/sdk/ReplayGain.h>
#include <memory>

namespace musik { namespace core { namespace library { namespace query {

    namespace tracks {
        static const std::string kColumns = "t.id, t.track, t.disc, t.bpm, t.duration, t.filesize, t.title, t.filename, t.thumbnail_id, al.name AS album, alar.name AS album_artist, gn.name AS genre, ar.name AS artist, t.filetime, t.visual_genre_id, t.visual_artist_id, t.album_artist_id, t.album_id, t.source_id, t.external_id, t.rating, replay_gain.album_gain, replay_gain.album_peak, replay_gain.track_gain, replay_gain.track_peak ";
        static const std::string kTables = "tracks t, albums al, artists alar, artists ar, genres gn";
        static const std::string kReplayGainJoin = "replay_gain ON t.id=replay_gain.track_id";
        static const std::string kPredicate = "t.album_id=al.id AND t.album_artist_id=alar.id AND t.visual_genre_id=gn.id AND t.visual_artist_id=ar.id";

        static const std::string kAllMetadataQueryById =
            "SELECT DISTINCT " + kColumns + " " +
            "FROM " + kTables + " " +
            "LEFT JOIN " + kReplayGainJoin + " " +
            "WHERE t.id=? AND " + kPredicate;

        static const std::string kAllMetadataQueryByIdBatch =
            "SELECT DISTINCT " + kColumns + " " +
            "FROM " + kTables + " " +
            "LEFT JOIN " + kReplayGainJoin + " " +
            "WHERE t.id IN ({{ids}}) AND " + kPredicate;

        static const std::string kAllMetadataQueryByExternalId =
            "SELECT DISTINCT " + kColumns + " " +
            "FROM " + kTables + " " +
            "LEFT JOIN " + kReplayGainJoin + " " +
            "WHERE t.external_id=? AND " + kPredicate;

        static const std::string kIdsOnlyQueryById =
            "SELECT DISTINCT external_id, source_id FROM tracks WHERE tracks.id=?";

        static const std::string kIdsOnlyQueryByExternalId =
            "SELECT DISTINCT external_id, source_id FROM tracks WHERE tracks.external_id=?";

        static inline void ParseFullTrackMetadata(
            musik::core::TrackPtr result,
            musik::core::db::Statement& trackQuery) 
        {
            result->SetValue(constants::Track::TRACK_NUM, trackQuery.ColumnText(1));
            result->SetValue(constants::Track::DISC_NUM, trackQuery.ColumnText(2));
            result->SetValue(constants::Track::BPM, trackQuery.ColumnText(3));
            result->SetValue(constants::Track::DURATION, trackQuery.ColumnText(4));
            result->SetValue(constants::Track::FILESIZE, trackQuery.ColumnText(5));
            result->SetValue(constants::Track::TITLE, trackQuery.ColumnText(6));
            result->SetValue(constants::Track::FILENAME, trackQuery.ColumnText(7));
            result->SetValue(constants::Track::THUMBNAIL_ID, trackQuery.ColumnText(8));
            result->SetValue(constants::Track::ALBUM, trackQuery.ColumnText(9));
            result->SetValue(constants::Track::ALBUM_ARTIST, trackQuery.ColumnText(10));
            result->SetValue(constants::Track::GENRE, trackQuery.ColumnText(11));
            result->SetValue(constants::Track::ARTIST, trackQuery.ColumnText(12));
            result->SetValue(constants::Track::FILETIME, trackQuery.ColumnText(13));
            result->SetValue(constants::Track::GENRE_ID, trackQuery.ColumnText(14));
            result->SetValue(constants::Track::ARTIST_ID, trackQuery.ColumnText(15));
            result->SetValue(constants::Track::ALBUM_ARTIST_ID, trackQuery.ColumnText(16));
            result->SetValue(constants::Track::ALBUM_ID, trackQuery.ColumnText(17));
            result->SetValue(constants::Track::SOURCE_ID, trackQuery.ColumnText(18));
            result->SetValue(constants::Track::EXTERNAL_ID, trackQuery.ColumnText(19));
            result->SetValue(constants::Track::RATING, trackQuery.ColumnText(20));

            musik::core::sdk::ReplayGain gain;
            gain.albumGain = trackQuery.IsNull(21) ? 1.0f : trackQuery.ColumnFloat(21);
            gain.albumPeak = trackQuery.IsNull(22) ? 1.0f : trackQuery.ColumnFloat(22);
            gain.trackGain = trackQuery.IsNull(23) ? 1.0f : trackQuery.ColumnFloat(23);
            gain.trackPeak = trackQuery.IsNull(24) ? 1.0f : trackQuery.ColumnFloat(24);
            result->SetReplayGain(gain);

            result->SetMetadataState(musik::core::sdk::MetadataState::Loaded);
        }

        static inline void ParseIdsOnlyTrackMetadata(
            musik::core::TrackPtr result,
            musik::core::db::Statement& trackQuery)
        {
            result->SetValue(constants::Track::EXTERNAL_ID, trackQuery.ColumnText(0));
            result->SetValue(constants::Track::SOURCE_ID, trackQuery.ColumnText(1));

            result->SetMetadataState(musik::core::sdk::MetadataState::Loaded);
        }

    }

} } } }
