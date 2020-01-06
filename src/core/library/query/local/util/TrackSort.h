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

#include <map>

namespace musik {
    namespace core {
        namespace db {
            namespace local {
                enum class TrackSortType : int {
                    Album = 0,
                    Title = 1,
                    Artist = 2,
                    DateAddedAsc = 3,
                    DateAddedDesc = 4,
                    DateUpdatedAsc = 5,
                    DateUpdatedDesc = 6,
                    LastPlayedAsc = 7,
                    LastPlayedDesc = 8,
                    RatingAsc = 9,
                    RatingDesc = 10,
                    PlayCountAsc = 11,
                    PlayCountDesc = 12,
                    Genre = 13,
                };

                static const std::map<TrackSortType, std::string> kTrackSortTypeToDisplayKey = {
                    { TrackSortType::Title, "track_list_sort_title" },
                    { TrackSortType::Album, "track_list_sort_album" },
                    { TrackSortType::Artist, "track_list_sort_artist" },
                    { TrackSortType::DateAddedAsc, "track_list_sort_date_added_asc" },
                    { TrackSortType::DateAddedDesc, "track_list_sort_date_added_desc" },
                    { TrackSortType::DateUpdatedAsc, "track_list_sort_date_updated_asc" },
                    { TrackSortType::DateUpdatedDesc, "track_list_sort_date_updated_desc" },
                    { TrackSortType::LastPlayedAsc, "track_list_sort_last_played_asc" },
                    { TrackSortType::LastPlayedDesc, "track_list_sort_last_played_desc" },
                    { TrackSortType::RatingAsc, "track_list_sort_rating_asc" },
                    { TrackSortType::RatingDesc, "track_list_sort_rating_desc" },
                    { TrackSortType::PlayCountAsc, "track_list_sort_play_count_asc" },
                    { TrackSortType::PlayCountDesc, "track_list_sort_play_count_desc" },
                    { TrackSortType::Genre, "track_list_sort_genre" },
                };
            }
        }
    }
}