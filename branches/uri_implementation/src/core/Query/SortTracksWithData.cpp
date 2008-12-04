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
#include <core/config_format.h>
#include <boost/algorithm/string.hpp>
#include <core/xml/ParserNode.h>
#include <core/xml/WriterNode.h>
#include <core/LibraryTrack.h>

using namespace musik::core;


//////////////////////////////////////////
///\brief
///Constructor
//////////////////////////////////////////
Query::SortTracksWithData::SortTracksWithData(void){
}

//////////////////////////////////////////
///\brief
///Destructor
//////////////////////////////////////////
Query::SortTracksWithData::~SortTracksWithData(void){
}


bool Query::SortTracksWithData::ParseQuery(Library::Base *library,db::Connection &db){

    std::vector<int> sortFieldsMetakeyId;

    // Create smart SQL statment
    std::string selectSQL("SELECT temp_track_sort.track_id ");
    std::string selectSQLTables("temp_track_sort JOIN tracks ON tracks.id=temp_track_sort.track_id ");
//    std::string selectSQLWhere(" WHERE 1=1 ");
    std::string selectSQLWhere(" ");
//    std::string selectSQLGroup(" GROUP BY tt.id ");
    std::string selectSQLGroup(" ");
    std::string selectSQLSort;

    std::string insertFields;
    std::string insertValues;
    std::string createTableStatement("CREATE TEMPORARY TABLE temp_track_sort (id INTEGER PRIMARY KEY AUTOINCREMENT,track_id INTEGER NOT NULL default 0)");

    db::CachedStatement selectMetaKeyId("SELECT id FROM meta_keys WHERE name=?",db);

    // Check if it's a fixed field
    if(musik::core::Library::Base::IsStaticMetaKey(this->sortByMetaKey)){
        selectSQL     += ",tracks."+this->sortByMetaKey;
        selectSQLSort += (selectSQLSort.empty()?" ORDER BY tracks.":",tracks.") + this->sortByMetaKey;

    // Check if it's a special MTO field
    }else if(musik::core::Library::Base::IsSpecialMTOMetaKey(this->sortByMetaKey) || musik::core::Library::Base::IsSpecialMTMMetaKey(this->sortByMetaKey)){
        if(this->sortByMetaKey=="album"){
            selectSQLTables += " LEFT OUTER JOIN albums ON albums.id=tracks.album_id ";
            selectSQL     += ",albums.name";
            selectSQLSort += (selectSQLSort.empty()?" ORDER BY albums.sort_order":",albums.sort_order");
        }
        if(this->sortByMetaKey=="visual_genre" || this->sortByMetaKey=="genre"){
            selectSQLTables += " LEFT OUTER JOIN genres ON genres.id=tracks.visual_genre_id ";
            selectSQL     += ",genres.name";
            selectSQLSort += (selectSQLSort.empty()?" ORDER BY genres.sort_order":",genres.sort_order");
        }
        if(this->sortByMetaKey=="visual_artist" || this->sortByMetaKey=="artist"){
            selectSQLTables += " LEFT OUTER JOIN artists ON artists.id=tracks.visual_artist_id";
            selectSQL     += ",artists.name";
            selectSQLSort += (selectSQLSort.empty()?" ORDER BY artists.sort_order":",artists.sort_order");
        }
    } else {
        // Sort by metakeys table
        selectMetaKeyId.BindText(0,this->sortByMetaKey);
        if(selectMetaKeyId.Step()==db::Row){
            sortFieldsMetakeyId.push_back(selectMetaKeyId.ColumnInt(0));

            selectSQLTables += " LEFT OUTER JOIN (SELECT track_meta.track_id,meta_values.content,meta_values.sort_order FROM track_meta,meta_values WHERE track_meta.meta_value_id=meta_values.id AND meta_values.meta_key_id="+boost::lexical_cast<std::string>(selectMetaKeyId.ColumnInt(0))+") the_meta ON the_meta.track_id=tracks.id ";
            selectSQL     += ",the_meta.content";
            selectSQLSort += (selectSQLSort.empty()?" ORDER BY the_meta.sort_order":",the_meta.sort_order");

        }
        selectMetaKeyId.Reset();
    }


    // First lets start by inserting all tracks in a temporary table
    db.Execute("DROP TABLE IF EXISTS temp_track_sort");

    db.Execute(createTableStatement.c_str());

    {
        insertValues    = "INSERT INTO temp_track_sort (track_id) VALUES (?)";
        db::Statement insertTracks(insertValues.c_str(),db);

        for(int i(0);i<this->tracksToSort.size();++i){
            DBINT track(this->tracksToSort[i]);
            insertTracks.BindInt(0,track);
            insertTracks.Step();
            insertTracks.Reset();
            insertTracks.UnBindAll();
        }
    }

    // Finaly keep sort order of inserted order.
    selectSQLSort   += ",temp_track_sort.id";

    std::string sql=selectSQL+" FROM "+selectSQLTables+selectSQLWhere+selectSQLGroup+selectSQLSort;

//    return this->ParseTracksSQL(sql,library,db);
    if(!library->QueryCanceled(this)){
        db::Statement selectTracks(sql.c_str(),db);

        TrackWithSortdataVector tempTrackResults;
        tempTrackResults.reserve(101);
        int row(0);
        while(selectTracks.Step()==db::ReturnCode::Row){
            TrackWithSortdata newSortData;
            newSortData.track.reset(new LibraryTrack(selectTracks.ColumnInt(0),library->Id()));
            newSortData.sortData    = selectTracks.ColumnTextUTF(1);

            tempTrackResults.push_back( newSortData );

            if( (++row)%100==0 ){
                boost::mutex::scoped_lock lock(library->resultMutex);
                this->trackResults.insert(this->trackResults.end(),tempTrackResults.begin(),tempTrackResults.end());

                tempTrackResults.clear();
                trackResults.reserve(101);
            }
        }
        if(!tempTrackResults.empty()){
            boost::mutex::scoped_lock lock(library->resultMutex);
            this->trackResults.insert(this->trackResults.end(),tempTrackResults.begin(),tempTrackResults.end());
        }

        return true;
    }else{
        return false;
    }
}

//////////////////////////////////////////
///\brief
///Copy a query
///
///\returns
///A shared_ptr to the Query::Base
//////////////////////////////////////////
Query::Ptr Query::SortTracksWithData::copy() const{
    Query::Ptr queryCopy(new Query::SortTracksWithData(*this));
    queryCopy->PostCopy();
    return queryCopy;
}

void Query::SortTracksWithData::AddTrack(DBINT trackId){
    this->tracksToSort.push_back(trackId);
}


void Query::SortTracksWithData::SortByMetaKey(std::string metaKey){
    this->sortByMetaKey = metaKey;
}

void Query::SortTracksWithData::ClearTracks(){
    this->tracksToSort.clear();
}

//////////////////////////////////////////
///\brief
///Recieve the query from XML
///
///\param queryNode
///Reference to query XML node
///
///The excpeted input format is like this:
///\code
///<query type="SortTracksWithData">
///   <tracks>1,3,5,7</tracks>
///</query>
///\endcode
///
///\returns
///true when successfully recieved
//////////////////////////////////////////
bool Query::SortTracksWithData::RecieveQuery(musik::core::xml::ParserNode &queryNode){

    while( musik::core::xml::ParserNode node = queryNode.ChildNode() ){
        if(node.Name()=="sortby"){
            node.WaitForContent();
            this->sortByMetaKey = node.Content();
        } else if(node.Name()=="tracks"){
            node.WaitForContent();

            typedef std::vector<std::string> StringVector;
            StringVector values;

            try{    // lexical_cast can throw
                boost::algorithm::split(values,node.Content(),boost::algorithm::is_any_of(","));
                for(StringVector::iterator value=values.begin();value!=values.end();++value){
                    this->tracksToSort.push_back( boost::lexical_cast<DBINT>(*value) );
                }
            }
            catch(...){
                return false;
            }

        }
    }
    return true;
}

std::string Query::SortTracksWithData::Name(){
    return "SortTracksWithData";
}

bool Query::SortTracksWithData::SendQuery(musik::core::xml::WriterNode &queryNode){
    {
        xml::WriterNode sortbyNode(queryNode,"sortby");
        sortbyNode.Content()    = this->sortByMetaKey;
    }
    {
        xml::WriterNode tracksNode(queryNode,"tracks");

        for(IntVector::iterator trackId=this->tracksToSort.begin();trackId!=this->tracksToSort.end();++trackId){
            if(!tracksNode.Content().empty()){
                tracksNode.Content().append(",");
            }
            tracksNode.Content().append(boost::lexical_cast<std::string>(*trackId));
        }
    }

    return true;
    
}

