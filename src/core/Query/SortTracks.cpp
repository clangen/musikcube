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
#include <core/Query/SortTracks.h>
#include <core/Library/Base.h>

using namespace musik::core;


//////////////////////////////////////////
///\brief
///Constructor
//////////////////////////////////////////
Query::SortTracks::SortTracks(void){
}

//////////////////////////////////////////
///\brief
///Destructor
//////////////////////////////////////////
Query::SortTracks::~SortTracks(void){
}


bool Query::SortTracks::ParseQuery(Library::Base *oLibrary,db::Connection &db){

    std::vector<int> sortFieldsMetakeyId;

    // Create smart SQL statment
    std::string selectSQL("SELECT tt.track_id FROM ");
    std::string selectSQLTables("temp_track_sort tt,tracks t");
    std::string selectSQLWhere(" WHERE tt.track_id=t.id ");
    std::string selectSQLGroup(" GROUP BY tt.id ");
    std::string selectSQLSort;

    std::string insertFields;
    std::string insertValues;
    std::string createTableStatement("CREATE TEMPORARY TABLE temp_track_sort (id INTEGER PRIMARY KEY AUTOINCREMENT,track_id INTEGER NOT NULL default 0");

    db::CachedStatement selectMetaKeyId("SELECT id FROM meta_keys WHERE name=?",db);

    while(!this->sortMetaKeys.empty()){
        std::string metakey   = this->sortMetaKeys.front();

        // Check if it's a fixed field
        if(musik::core::Library::Base::IsStaticMetaKey(metakey)){
            selectSQLSort += (selectSQLSort.empty()?" ORDER BY t.":",t.") + metakey;

        // Check if it's a special MTO field
        }else if(musik::core::Library::Base::IsSpecialMTOMetaKey(metakey) || musik::core::Library::Base::IsSpecialMTMMetaKey(metakey)){
            if(metakey=="album"){
                selectSQLTables += ",albums al";
                selectSQLWhere  += "al.id=t.album_id";
                selectSQLSort += (selectSQLSort.empty()?" ORDER BY al.sort_order":",al.sort_order");
            }
            if(metakey=="visual_genre" || metakey=="genre"){
                selectSQLTables += ",genres g";
                selectSQLWhere  += "g.id=t.visual_genre_id";
                selectSQLSort += (selectSQLSort.empty()?" ORDER BY g.sort_order":",g.sort_order");
            }
            if(metakey=="visual_artist" || metakey=="artist"){
                selectSQLTables += ",artists ar";
                selectSQLWhere  += "ar.id=t.visual_artist_id";
                selectSQLSort += (selectSQLSort.empty()?" ORDER BY ar.sort_order":",ar.sort_order");
            }
        } else {
            // Sort by metakeys table
            selectMetaKeyId.BindText(0,metakey);
            if(selectMetaKeyId.Step()==db::Row){
                sortFieldsMetakeyId.push_back(selectMetaKeyId.ColumnInt(0));

                std::string sortField = boost::str( boost::format("ef%1%")%(sortFieldsMetakeyId.size()-1) );

                selectSQLSort += (selectSQLSort.empty()?" ORDER BY tt.":",tt.")+sortField;
                createTableStatement += ","+sortField+" INTEGER";
                insertFields    += ","+sortField;
                insertValues    += ",?";

            }
            selectMetaKeyId.Reset();

        }

        this->sortMetaKeys.pop_front();
    }


    // First lets start by inserting all tracks in a temporary table
    db.Execute("DROP TABLE IS EXISTS temp_track_sort");

    createTableStatement+=")";
    db.Execute(createTableStatement.c_str());

    {
        insertValues    = "INSERT INTO temp_track_sort (track_id"+insertFields+") VALUES (?"+insertValues+")";
        db::Statement insertTracks(insertValues.c_str(),db);

        db::Statement selectMetaValue("SELECT mv.sort_order FROM meta_values mv,track_meta tm WHERE tm.meta_value_id=mv.id AND tm.track_id=? AND mv.meta_key_id=? LIMIT 1",db);
        for(int i(0);i<this->tracksToSort.size();++i){
            int track(this->tracksToSort[i]);
            insertTracks.BindInt(0,track);

            // Lets find the meta values
            //sortFieldsMetakeyId
            for(int field(0);field<sortFieldsMetakeyId.size();++field){
                int metakeyId(sortFieldsMetakeyId[field]);
                selectMetaValue.BindInt(0,track);
                selectMetaValue.BindInt(1,metakeyId);
                if(selectMetaValue.Step()==db::Row){
                    insertTracks.BindInt(field+1,selectMetaValue.ColumnInt(0));
                }
            }

            insertTracks.Step();
            insertTracks.Reset();
            insertTracks.UnBindAll();
        }
    }


    std::string sql=selectSQL+selectSQLTables+selectSQLWhere+selectSQLGroup+selectSQLSort;
    db::Statement selectTracks(sql.c_str(),db);

    while(selectTracks.Step()==db::Row){
        boost::mutex::scoped_lock lock(oLibrary->oResultMutex);
        this->trackResults.push_back(TrackPtr(new Track(selectTracks.ColumnInt(0))));
    }
    return true;
}

//////////////////////////////////////////
///\brief
///Copy a query
///
///\returns
///A shared_ptr to the Query::Base
//////////////////////////////////////////
Query::Ptr Query::SortTracks::copy() const{
    return Query::Ptr(new Query::SortTracks(*this));
}

void Query::SortTracks::AddTrack(int trackId){
    this->tracksToSort.push_back(trackId);
}

void Query::SortTracks::AddTracks(std::vector<int> &tracks){
    this->tracksToSort.reserve(this->tracksToSort.size()+tracks.size());
    for(std::vector<int>::iterator track=tracks.begin();track!=tracks.end();++track){
        this->tracksToSort.push_back(*track);
    }
}

void Query::SortTracks::AddTracks(musik::core::tracklist::IRandomAccess &tracks){
    this->tracksToSort.reserve(this->tracksToSort.size()+tracks.Size());
    for(int i(0);i<tracks.Size();++i){
        this->tracksToSort.push_back(tracks.Track(i)->id);
    }

}

void Query::SortTracks::SortByMetaKeys(std::list<std::string> metaKeys){
    this->sortMetaKeys  = metaKeys;
}

