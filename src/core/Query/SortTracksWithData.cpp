//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
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

#include <core/Query/SortTracksWithData.h>
#include <core/Library/Base.h>
#include <core/config.h>
#include <boost/algorithm/string.hpp>
#include <core/LibraryTrack.h>
#include <core/Common.h>

using namespace musik::core;
using namespace musik::core::query;

//////////////////////////////////////////
///\brief
///Constructor
//////////////////////////////////////////
SortTracksWithData::SortTracksWithData()
: clearedTrackResults(false) {
}

//////////////////////////////////////////
///\brief
///Destructor
//////////////////////////////////////////
SortTracksWithData::~SortTracksWithData() {
}

//////////////////////////////////////////
///\brief
///Copy a query
///
///\returns
///A shared_ptr to the Base
//////////////////////////////////////////
Ptr SortTracksWithData::copy() const {
    Ptr queryCopy(new SortTracksWithData(*this));
    queryCopy->PostCopy();
    return queryCopy;
}

//////////////////////////////////////////
///\brief
///Add a track (trackId) in for sorting
//////////////////////////////////////////
void SortTracksWithData::AddTrack(DBID trackId){
    this->tracksToSort.push_back(trackId);
}


//////////////////////////////////////////
///\brief
///What metakey to sort by
//////////////////////////////////////////
void SortTracksWithData::SortByMetaKey(std::string metaKey){
    this->sortByMetaKey = metaKey;
}

//////////////////////////////////////////
///\brief
///If you are reusing the query, clear what tracks to sort by
//////////////////////////////////////////
void SortTracksWithData::ClearTracks(){
    this->tracksToSort.clear();
}

//////////////////////////////////////////
///\brief
///The name ("SortTracksWithData") of the query. 
//////////////////////////////////////////
std::string SortTracksWithData::Name(){
    return "SortTracksWithData";
}

//////////////////////////////////////////
///\brief
///Execute the callbacks. In this case the "TrackResults" signal
//////////////////////////////////////////
bool SortTracksWithData::RunCallbacks(library::Base *library){

    bool bReturn(false);

    TrackWithSortdataVector trackResultsCopy;

    {    // Scope for swapping the results safely
        boost::mutex::scoped_lock lock(library->resultMutex);
        trackResultsCopy.swap(this->trackResults);
    }

    {
        boost::mutex::scoped_lock lock(library->libraryMutex);
        if( (this->status & Base::Ended)!=0){
            // If the query is finished, this function should return true to report that it is finished.
            bReturn    = true;
        }
    }


    // Check for Tracks
    if( !trackResultsCopy.empty() ){

        // Call the slots
        this->TrackResults(&trackResultsCopy,!this->clearedTrackResults);

        if(!this->clearedTrackResults){
            this->clearedTrackResults    = true;
        }

    }

    return bReturn;
}

//////////////////////////////////////////
///\brief
///Operator to be able to sort using the sortData
//////////////////////////////////////////
bool SortTracksWithData::TrackWithSortdata::operator<(const TrackWithSortdata &trackWithSortData) const{
    return this->sortData < trackWithSortData.sortData;
}

//////////////////////////////////////////
///\brief
///Executes the query, meaning id does all the querying to the database.
//////////////////////////////////////////
bool SortTracksWithData::ParseQuery(library::Base *library, db::Connection &db) {
    // Create smart SQL statment
    std::string selectSQL("SELECT temp_track_sort.track_id ");
    std::string selectSQLTables("temp_track_sort JOIN tracks ON tracks.id=temp_track_sort.track_id ");
    std::string selectSQLSort;

    db::CachedStatement selectMetaKeyId("SELECT id FROM meta_keys WHERE name=?", db);

    // Check if it's a fixed field
    if (musik::core::library::Base::IsStaticMetaKey(this->sortByMetaKey)) {
        selectSQL += ",tracks." + this->sortByMetaKey;
        selectSQLSort += (selectSQLSort.empty() ? " ORDER BY tracks." : ",tracks.") + this->sortByMetaKey;

        // Check if it's a special MTO (many to one relation) field
    }
    else if (musik::core::library::Base::IsSpecialMTOMetaKey(this->sortByMetaKey) || musik::core::library::Base::IsSpecialMTMMetaKey(this->sortByMetaKey)) {
        if (this->sortByMetaKey == "album") {
            selectSQLTables += " LEFT OUTER JOIN albums ON albums.id=tracks.album_id ";
            selectSQL += ",albums.name";
            selectSQLSort += (selectSQLSort.empty() ? " ORDER BY albums.sort_order" : ",albums.sort_order");
        }
        if (this->sortByMetaKey == "visual_genre" || this->sortByMetaKey == "genre") {
            selectSQLTables += " LEFT OUTER JOIN genres ON genres.id=tracks.visual_genre_id ";
            selectSQL += ",genres.name";
            selectSQLSort += (selectSQLSort.empty() ? " ORDER BY genres.sort_order" : ",genres.sort_order");
        }
        if (this->sortByMetaKey == "visual_artist" || this->sortByMetaKey == "artist") {
            selectSQLTables += " LEFT OUTER JOIN artists ON artists.id=tracks.visual_artist_id";
            selectSQL += ",artists.name";
            selectSQLSort += (selectSQLSort.empty() ? " ORDER BY artists.sort_order" : ",artists.sort_order");
        }
    }
    else {
        // Sort by metakeys table
        selectMetaKeyId.BindText(0, this->sortByMetaKey);
        if (selectMetaKeyId.Step() == db::Row) {
            selectSQLTables += " LEFT OUTER JOIN (SELECT track_meta.track_id,meta_values.content,meta_values.sort_order FROM track_meta,meta_values WHERE track_meta.meta_value_id=meta_values.id AND meta_values.meta_key_id=" + boost::lexical_cast<std::string>(selectMetaKeyId.ColumnInt(0)) + ") the_meta ON the_meta.track_id=tracks.id ";
            selectSQL += ",the_meta.content";
            selectSQLSort += (selectSQLSort.empty() ? " ORDER BY the_meta.sort_order" : ",the_meta.sort_order");

        }
        selectMetaKeyId.Reset();
    }


    // First lets start by inserting all tracks in a temporary table
    db.Execute("DROP TABLE IF EXISTS temp_track_sort");
    db.Execute("CREATE TEMPORARY TABLE temp_track_sort (id INTEGER PRIMARY KEY AUTOINCREMENT,track_id INTEGER NOT NULL default 0)");

    {
        db::Statement insertTracks("INSERT INTO temp_track_sort (track_id) VALUES (?)", db);

        for (std::size_t i(0); i < this->tracksToSort.size(); ++i) {
            DBID track(this->tracksToSort[i]);
            insertTracks.BindInt(0, track);
            insertTracks.Step();
            insertTracks.Reset();
            insertTracks.UnBindAll();
        }
    }

    // Finaly keep sort order of inserted order.
    selectSQLSort += ",temp_track_sort.id";


    ////////////////////////////////////////////////////////
    // The main SQL query what this class is all about
    std::string sql = selectSQL + " FROM " + selectSQLTables + selectSQLSort;


    if (!library->QueryCanceled(this)) {
        db::Statement selectTracks(sql.c_str(), db);

        TrackWithSortdataVector tempTrackResults;
        int row(0);
        while (selectTracks.Step() == db::Row) {
            TrackWithSortdata newSortData;
            newSortData.track.reset(new LibraryTrack(selectTracks.ColumnInt(0), library->Id()));
            const char* sortDataPtr = selectTracks.ColumnText(1);
            if (sortDataPtr) {
                newSortData.sortData = sortDataPtr;
            }

            // Convert the content to lower if futher sorting need to be done
            boost::algorithm::to_lower(newSortData.sortData);

            tempTrackResults.push_back(newSortData);

            // Each 100 result, lock the mutex and insert the results
            if ((++row) % 100 == 0) {
                boost::mutex::scoped_lock lock(library->resultMutex);
                this->trackResults.insert(this->trackResults.end(), tempTrackResults.begin(), tempTrackResults.end());
                tempTrackResults.clear();
            }
        }

        // If there are any results not inserted, insert now
        if (!tempTrackResults.empty()) {
            boost::mutex::scoped_lock lock(library->resultMutex);
            this->trackResults.insert(this->trackResults.end(), tempTrackResults.begin(), tempTrackResults.end());
        }

        // All done succesfully, return true
        return true;
    }
    else {
        return false;
    }
}