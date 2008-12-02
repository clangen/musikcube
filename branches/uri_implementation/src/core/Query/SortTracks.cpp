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
#include <core/config_format.h>
#include <boost/algorithm/string.hpp>
#include <core/xml/ParserNode.h>
#include <core/xml/WriterNode.h>

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


bool Query::SortTracks::ParseQuery(Library::Base *library,db::Connection &db){

    std::vector<int> sortFieldsMetakeyId;

    // Create smart SQL statment
    std::string selectSQL("SELECT temp_track_sort.track_id,tracks.duration,tracks.filesize ");
    std::string selectSQLTables("temp_track_sort LEFT OUTER JOIN tracks ON tracks.id=temp_track_sort.track_id ");
//    std::string selectSQLWhere(" WHERE 1=1 ");
    std::string selectSQLWhere(" ");
//    std::string selectSQLGroup(" GROUP BY tt.id ");
    std::string selectSQLGroup(" ");
    std::string selectSQLSort;

    std::string insertFields;
    std::string insertValues;
    std::string createTableStatement("CREATE TEMPORARY TABLE temp_track_sort (id INTEGER PRIMARY KEY AUTOINCREMENT,track_id INTEGER NOT NULL default 0");

    db::CachedStatement selectMetaKeyId("SELECT id FROM meta_keys WHERE name=?",db);

    while(!this->sortMetaKeys.empty()){
        std::string metakey   = this->sortMetaKeys.front();

        // Check if it's a fixed field
        if(musik::core::Library::Base::IsStaticMetaKey(metakey)){
//            selectSQL     += ",tracks."+metakey;
            selectSQLSort += (selectSQLSort.empty()?" ORDER BY tracks.":",tracks.") + metakey;

        // Check if it's a special MTO field
        }else if(musik::core::Library::Base::IsSpecialMTOMetaKey(metakey) || musik::core::Library::Base::IsSpecialMTMMetaKey(metakey)){
            if(metakey=="album"){
                selectSQLTables += " LEFT OUTER JOIN albums ON albums.id=tracks.album_id ";
//                selectSQLTables += " albums al ";
//                selectSQLWhere  += "al.id=t.album_id";
//                selectSQL     += ",albums.sort_order";
                selectSQLSort += (selectSQLSort.empty()?" ORDER BY albums.sort_order":",albums.sort_order");
            }
            if(metakey=="visual_genre" || metakey=="genre"){
                selectSQLTables += " LEFT OUTER JOIN genres ON genres.id=tracks.visual_genre_id ";
//                selectSQLTables += ",genres g";
//                selectSQLWhere  += "g.id=t.visual_genre_id";
//                selectSQL     += ",genres.sort_order";
                selectSQLSort += (selectSQLSort.empty()?" ORDER BY genres.sort_order":",genres.sort_order");
            }
            if(metakey=="visual_artist" || metakey=="artist"){
                selectSQLTables += " LEFT OUTER JOIN artists ON artists.id=tracks.visual_artist_id";
//                selectSQLTables += ",artists ar";
//                selectSQLWhere  += "ar.id=t.visual_artist_id";
//                selectSQL     += ",artists.sort_order";
                selectSQLSort += (selectSQLSort.empty()?" ORDER BY artists.sort_order":",artists.sort_order");
            }
        } else {
            // Sort by metakeys table
            selectMetaKeyId.BindText(0,metakey);
            if(selectMetaKeyId.Step()==db::Row){
                sortFieldsMetakeyId.push_back(selectMetaKeyId.ColumnInt(0));

                std::string sortField = boost::str( boost::format("ef%1%")%(sortFieldsMetakeyId.size()-1) );

                selectSQLSort += (selectSQLSort.empty()?" ORDER BY temp_track_sort.":",temp_track_sort.")+sortField;
                createTableStatement += ","+sortField+" INTEGER";
                insertFields    += ","+sortField;
                insertValues    += ",?";

            }
            selectMetaKeyId.Reset();

        }

        this->sortMetaKeys.pop_front();
    }


    // First lets start by inserting all tracks in a temporary table
    db.Execute("DROP TABLE IF EXISTS temp_track_sort");

    createTableStatement+=")";
    db.Execute(createTableStatement.c_str());

    {
        insertValues    = "INSERT INTO temp_track_sort (track_id"+insertFields+") VALUES (?"+insertValues+")";
        db::Statement insertTracks(insertValues.c_str(),db);

        db::Statement selectMetaValue("SELECT mv.sort_order FROM meta_values mv,track_meta tm WHERE tm.meta_value_id=mv.id AND tm.track_id=? AND mv.meta_key_id=? LIMIT 1",db);
        for(int i(0);i<this->tracksToSort.size();++i){
            DBINT track(this->tracksToSort[i]);
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

    // Finaly keep sort order of inserted order.
    selectSQLSort   += ",temp_track_sort.id";

    std::string sql=selectSQL+" FROM "+selectSQLTables+selectSQLWhere+selectSQLGroup+selectSQLSort;

    return this->ParseTracksSQL(sql,library,db);
}

//////////////////////////////////////////
///\brief
///Copy a query
///
///\returns
///A shared_ptr to the Query::Base
//////////////////////////////////////////
Query::Ptr Query::SortTracks::copy() const{
    Query::Ptr queryCopy(new Query::SortTracks(*this));
    queryCopy->PostCopy();
    return queryCopy;
}

void Query::SortTracks::AddTrack(DBINT trackId){
    this->tracksToSort.push_back(trackId);
}

void Query::SortTracks::AddTracks(std::vector<DBINT> &tracks){
    this->tracksToSort.reserve(this->tracksToSort.size()+tracks.size());
    for(std::vector<DBINT>::iterator track=tracks.begin();track!=tracks.end();++track){
        this->tracksToSort.push_back(*track);
    }
}

void Query::SortTracks::AddTracks(musik::core::tracklist::LibraryList &tracks){
    this->tracksToSort.reserve(this->tracksToSort.size()+tracks.Size());
    for(int i(0);i<tracks.Size();++i){
        this->tracksToSort.push_back(tracks[i]->Id());
    }

}

void Query::SortTracks::SortByMetaKeys(std::list<std::string> metaKeys){
    this->sortMetaKeys  = metaKeys;
}

void Query::SortTracks::ClearTracks(){
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
///<query type="SortTracks">
///   <tracks>1,3,5,7</tracks>
///</query>
///\endcode
///
///\returns
///true when successfully recieved
//////////////////////////////////////////
bool Query::SortTracks::RecieveQuery(musik::core::xml::ParserNode &queryNode){

    while( musik::core::xml::ParserNode node = queryNode.ChildNode() ){
        if(node.Name()=="sortby"){
            node.WaitForContent();
            try{
                // Split list directly into the sortMetaKeys
                boost::algorithm::split(this->sortMetaKeys,node.Content(),boost::algorithm::is_any_of(","));
            }
            catch(...){
                return false;
            }

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

        }else{
            this->RecieveQueryStandardNodes(node);
        }
    }
    return true;
}

std::string Query::SortTracks::Name(){
    return "SortTracks";
}

bool Query::SortTracks::SendQuery(musik::core::xml::WriterNode &queryNode){
    {
        xml::WriterNode sortbyNode(queryNode,"sortby");

        for(StringList::iterator metaKey=this->sortMetaKeys.begin();metaKey!=this->sortMetaKeys.end();++metaKey){
            if(!sortbyNode.Content().empty()){
                sortbyNode.Content().append(",");
            }
            sortbyNode.Content().append(*metaKey);
        }
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

    this->SendQueryStandardNodes(queryNode);

    return true;
    
}

