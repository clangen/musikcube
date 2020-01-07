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
#include <core/db/Statement.h>
#include <core/db/Connection.h>
#include <memory>

namespace musik { namespace core { namespace db { namespace local {

    namespace category {
        namespace constants = musik::core::library::constants;

        /* there are two types of track resources in our app: Regular, and Extended. "Regular"
        properties are well-defined and optimized, and include album, artist, album artist,
        and genre. they are fast to retrieve and easy to query. however, we also allow for
        plugins to index arbitrary track metadata in a completely denormalized key/value
        store. these resource types are called "Extended" properties. */

        enum class PropertyType: int { Regular, Extended };

        /* property name to foreign key id */
        static std::map<std::string, std::string> PREDICATE_TO_COLUMN_MAP = {
            { constants::Track::ALBUM, "album_id" },
            { constants::Track::ARTIST, "visual_artist_id" },
            { constants::Track::ALBUM_ARTIST, "album_artist_id" },
            { constants::Track::GENRE, "visual_genre_id" },
            { constants::Track::DIRECTORY, "directory_id" }
        };

        /* resource type to a pair { <table_name>, <track_table_fk_name> } */
        static std::map<std::string, std::pair<std::string, std::string>> REGULAR_PROPERTY_MAP = {
            { "album", { "albums", "album_id" } },
            { "artist", { "artists", "visual_artist_id" } },
            { "album_artist", { "artists", "album_artist_id" } },
            { "genre", { "genres", "visual_genre_id" } },
            { "directory", { "directories", "directory_id" } }
        };

        static const std::string REGULAR_PREDICATE = " tracks.{{fk_id}}=? ";
        static const std::string REGULAR_FILTER = " AND LOWER({{table}}.name) LIKE ? ";

        static const std::string EXTENDED_PREDICATE = " (key=? AND meta_value_id=?) ";
        static const std::string EXTENDED_FILTER = " AND LOWER(extended_metadata.value) LIKE ?";

        static const std::string EXTENDED_INNER_JOIN =
            "INNER JOIN ( "
            "  SELECT id AS track_id "
            "  FROM extended_metadata "
            "  WHERE {{extended_predicates}} "
            "  GROUP BY track_id "
            "  HAVING COUNT(track_id)={{extended_predicate_count}} "
            ") AS md ON tracks.id=md.track_id ";

        /* REGULAR_PROPERTY_QUERY is used to return a list of property values and their
        respective IDs for regular properties, that is, albums, artists, album artists,
        and genres. these resource types are the most common, and are optimized for display.
        the query may also be predicated to select all resources that are related to other
        resources, including extended resources. the full requerly propety looks something
        like the following example where we query all albums made in "1995" by "J. Cantrell": */

        // SELECT DISTINCT albums.id, albums.name
        //     FROM albums, tracks
        //     INNER JOIN(
        //         SELECT id AS track_id
        //         FROM extended_metadata
        //         WHERE
        //         (key = "year" AND value = "1995") OR
        //         (key = "composer" AND value = "J. Cantrell")
        //         GROUP BY track_id
        //         HAVING COUNT(track_id) = 2
        //     ) AS md ON tracks.id = md.track_id
        //     WHERE
        //         albums.id = tracks.album_id AND
        //         tracks.visible = 1;

        static const std::string REGULAR_PROPERTY_QUERY =
            "SELECT DISTINCT {{table}}.id, {{table}}.name "
            "FROM {{table}}, tracks "
            "{{extended_predicates}} "
            "WHERE {{table}}.id=tracks.{{fk_id}} AND tracks.visible=1 "
            "{{regular_predicates}} "
            "{{regular_filter}} "
            "ORDER BY {{table}}.sort_order";

        /* EXTENDED_PROPERTY_QUERY is similar to REGULAR_PROPERTY_QUERY, but is used to
        retrieve non-standard metadata fields. it's slower, has (potentially) more joins,
        and is generally more difficult to use. here's an example where we select all
        "years" for a particular artist "595" where the composer is "J. Cantrell": */

        // SELECT DISTINCT meta_value_id, value
        //     FROM extended_metadata
        //     INNER JOIN(
        //         SELECT id AS track_id
        //         FROM tracks
        //         WHERE visual_artist_id = 595
        //     ) AS reg ON extended_metadata.id = reg.track_id
        //     INNER JOIN(
        //         SELECT id AS track_id
        //         FROM extended_metadata
        //         WHERE
        //             (key = "composer" AND value = "J. Cantrell") OR
        //             ...
        //         GROUP BY track_id
        //         HAVING COUNT(track_id) = 1
        //     ) AS md ON extended_metadata.id = md.track_id
        //     WHERE
        //         extended_metadata.key = "year";

        static const std::string EXTENDED_PROPERTY_QUERY =
            "SELECT DISTINCT meta_value_id, value "
            "FROM extended_metadata "
            "INNER JOIN ( "
            "  SELECT id AS track_id "
            "  FROM tracks "
            "  WHERE tracks.visible=1 {{regular_predicates}} "
            ") AS reg on extended_metadata.id=reg.track_id "
            "{{extended_predicates}} "
            "WHERE "
            "  extended_metadata.key=? "
            "  {{extended_filter}} "
            "ORDER BY extended_metadata.value ASC";

        /* used to select all tracks that match a specified set of predicates. both
        regular and extended predicates are supported. in essense: */

        // SELECT DISTINCT tracks.*
        //     FROM tracks
        //     INNER JOIN(
        //         SELECT id AS track_id
        //         FROM extended_metadata
        //         WHERE
        //             (key = "year" AND value = "1995") OR
        //             (key = "composer" AND value = "J. Cantrell")
        //         GROUP BY track_id
        //         HAVING COUNT(track_id) = 2
        //     ) AS md ON tracks.id = md.track_id;

        static const std::string CATEGORY_TRACKLIST_FILTER =
            " AND (tracks.title LIKE ? OR al.name LIKE ? OR ar.name LIKE ? OR gn.name LIKE ?) ";

        /* note: al.name needs to be the second column selected to ensure proper grouping by
        album in the UI layer! */
        static const std::string CATEGORY_TRACKLIST_QUERY =
            "SELECT DISTINCT tracks.id, al.name, tracks.date_added, tracks.date_updated, tracks.last_played, tracks.play_count, tracks.rating "
            "FROM tracks, albums al, artists ar, genres gn "
            "{{extended_predicates}} "
            "WHERE "
            "  tracks.visible=1 AND "
            "  tracks.album_id=al.id AND "
            "  tracks.visual_genre_id=gn.id AND "
            "  tracks.visual_artist_id=ar.id "
            "  {{regular_predicates}} "
            "  {{tracklist_filter}} "
            "{{order_by}} "
            "{{limit_and_offset}} ";

        /* ALBUM_LIST_QUERY is like a specialized REGULAR_PROPERTY_QUERY used by
        LocalMetadataProxy to return album resources with thumbnail, artist,
        and other supplementary information. */

        static const std::string ALBUM_LIST_FILTER =
            " AND (LOWER(album) like ? OR LOWER(album_artist) like ?) ";

        static const std::string ALBUM_LIST_QUERY =
            "SELECT DISTINCT "
            "  albums.id, "
            "  albums.name as album, "
            "  tracks.album_artist_id, "
            "  artists.name as album_artist, "
            "  albums.thumbnail_id "
            "FROM albums, tracks, artists "
            "{{extended_predicates}} "
            "WHERE "
            "  albums.id = tracks.album_id AND "
            "  artists.id = tracks.album_artist_id AND "
            "  tracks.visible=1 "
            "  {{regular_predicates}} "
            "  {{album_list_filter}} "
            "ORDER BY albums.name ASC ";

        /* data types */

        using Predicate = std::pair<std::string, int64_t>;
        using PredicateList = std::vector<Predicate>;
        struct Argument { virtual void Bind(Statement& stmt, int pos) const = 0; };
        using ArgumentList = std::vector<std::shared_ptr<Argument>>;

        /* functions */

        extern PropertyType GetPropertyType(const std::string& key);

        extern std::shared_ptr<Argument> IdArgument(int64_t);
        extern std::shared_ptr<Argument> StringArgument(const std::string);

        extern size_t Hash(const PredicateList& input);

        extern void ReplaceAll(
            std::string& input,
            const std::string& find,
            const std::string& replace);

        extern void SplitPredicates(
            const PredicateList& input,
            PredicateList& regular,
            PredicateList& extended);

        extern std::string JoinRegular(
            const PredicateList& pred,
            ArgumentList& args,
            const std::string& prefix = "");

        extern std::string InnerJoinExtended(
            const PredicateList& pred, ArgumentList& args);

        extern std::string JoinExtended(
            const PredicateList& pred, ArgumentList& args);

        extern void Apply(
            musik::core::db::Statement& stmt,
            const ArgumentList& args);
    }

} } } }
