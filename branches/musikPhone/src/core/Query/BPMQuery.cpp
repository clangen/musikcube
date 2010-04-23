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
#include <core/Query/BPMQuery.h>
#include <core/Library/Base.h>
#include <core/xml/ParserNode.h>
#include <core/xml/WriterNode.h>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

using namespace musik::core;


//////////////////////////////////////////
///\brief
///Constructor
//////////////////////////////////////////
Query::BPMQuery::BPMQuery(void) : ListSelection(){
}

//////////////////////////////////////////
///\brief
///Destructor
//////////////////////////////////////////
Query::BPMQuery::~BPMQuery(void){
}

bool Query::BPMQuery::ParseQuery(Library::Base *library,db::Connection &db){

    bool success(true);

    ////////////////////////////////////////////////
    // First lets make sure we do not have any signals
    // or selections that are empty
    for(MetadataSignals::iterator signal=this->metadataEvent.begin();signal!=this->metadataEvent.end();){
        if( !signal->second.has_connections() ){
            this->metadataEvent.erase(signal);
            signal=this->metadataEvent.begin();
        }else{
            ++signal;
        }
    }

    for(SelectedMetadata::iterator selected=this->selectedMetadata.begin();selected!=this->selectedMetadata.end();){
        if( selected->second.empty() ){
            this->selectedMetadata.erase(selected);
            selected    = this->selectedMetadata.begin();
        }else{
            ++selected;
        }
    }

    ////////////////////////////////////////////////
    // Second. Select track
    std::string sqlSelectTrack("SELECT t.id AS id,t.duration AS sum_duration,t.filesize AS sum_filesize");
    std::string sqlSelectTrackFrom(" FROM tracks t ");
    std::string sqlSelectTrackWhere(" WHERE t.bpm>=");
	sqlSelectTrackWhere	+= boost::lexical_cast<std::string>(this->bpm);
	sqlSelectTrackWhere	+= " ";
    std::string sqlSelectTrackOrder("ORDER BY t.bpm ASC LIMIT 20");

    // Copy selected metakeys
    std::set<std::string> metakeysSelected,metakeysSelectedCopy;
    for(SelectedMetadata::iterator selected=this->selectedMetadata.begin();selected!=this->selectedMetadata.end();++selected){
        metakeysSelected.insert(selected->first);
    }
    metakeysSelectedCopy    = metakeysSelected;

    // Copy queried metakeys
    std::set<std::string> metakeysQueried,metakeysQueriedCopy;
    for(MetadataSignals::iterator signal=this->metadataEvent.begin();signal!=this->metadataEvent.end();++signal){
        if(metakeysSelected.find(signal->first)==metakeysSelected.end()){    // Do not insert metakeys that are selected
            metakeysQueried.insert(signal->first);
        }
    }
    metakeysQueriedCopy        = metakeysQueried;

    if(!metakeysSelected.empty()){
        ////////////////////////////////////////////////
        // Selected genre
        this->SQLSelectQuery("genre","t.id IN (SELECT track_id FROM track_genres WHERE genre_id IN (",")) ",metakeysSelected,sqlSelectTrackWhere,library);

        ////////////////////////////////////////////////
        // Selected artists
        this->SQLSelectQuery("artist","t.id IN (SELECT track_id FROM track_artists WHERE artist_id IN (",")) ",metakeysSelected,sqlSelectTrackWhere,library);

        ////////////////////////////////////////////////
        // Selected albums
        this->SQLSelectQuery("album","t.album_id IN (",") ",metakeysSelected,sqlSelectTrackWhere,library);

        ////////////////////////////////////////////////
        // Selected year
        this->SQLSelectQuery("year","t.year IN (",") ",metakeysSelected,sqlSelectTrackWhere,library);

        ////////////////////////////////////////////////
        // Selected duration
        this->SQLSelectQuery("duration","t.duration IN (",") ",metakeysSelected,sqlSelectTrackWhere,library);

        ////////////////////////////////////////////////
        // Selected track
        this->SQLSelectQuery("track","t.track IN (",") ",metakeysSelected,sqlSelectTrackWhere,library);

        ////////////////////////////////////////////////
        // Selected metadata
        std::set<std::string> tempMetakeysSelected(metakeysSelected);
        for(std::set<std::string>::iterator metakey=tempMetakeysSelected.begin();metakey!=tempMetakeysSelected.end();++metakey){
            this->SQLSelectQuery(metakey->c_str(),"t.id IN (SELECT track_id FROM track_meta WHERE meta_value_id IN (",")) ",metakeysSelected,sqlSelectTrackWhere,library);
        }

    }

    ////////////////////////////////////////////////
    // check for fixed fields that are queried and add them to the selected fields
    if(metakeysQueried.find("year")!=metakeysQueried.end()){
        sqlSelectTrack.append(",t.year AS year");
    }
    if(metakeysQueried.find("bpm")!=metakeysQueried.end()){
        sqlSelectTrack.append(",t.bpm AS bpm");
    }
    if(metakeysQueried.find("album")!=metakeysQueried.end()){
        sqlSelectTrack.append(",t.album_id AS album_id");
    }
    if(metakeysQueried.find("duration")!=metakeysQueried.end()){
        sqlSelectTrack.append(",t.duration AS duration");
    }
    if(metakeysQueried.find("filesize")!=metakeysQueried.end()){
        sqlSelectTrack.append(",t.filesize AS filesize");
    }
    if(metakeysQueried.find("track")!=metakeysQueried.end()){
        sqlSelectTrack.append(",t.track AS track");
    }

    ////////////////////////////////////////////////
    // Lets get the tracks from the database and insert into a temporary table
    std::string sqlTracks = sqlSelectTrack+sqlSelectTrackFrom+sqlSelectTrackWhere+sqlSelectTrackOrder;
    if(!metakeysSelectedCopy.empty()){
        // Select into temporary table
        sqlTracks    = "CREATE TEMPORARY TABLE temp_tracks_list AS ";
        sqlTracks.append(sqlSelectTrack+sqlSelectTrackFrom+sqlSelectTrackWhere+sqlSelectTrackOrder);

        // Drop table if last Query::ListSelection was canceled
        db.Execute("DROP TABLE IF EXISTS temp_tracks_list");
        db.Execute(sqlTracks.c_str());
    }


    ////////////////////////////////////////////////
    // Third. Get the metadata values for connected slots.
    //    std::string sqlTrackQuery(sqlTracks);

    // Genre
    if(metakeysSelectedCopy.empty()){
        this->QueryForMetadata("genre","SELECT g.id,g.name FROM genres g WHERE g.aggregated=0 ORDER BY g.sort_order",metakeysQueried,library,db);
    }else{
        this->QueryForMetadata("genre","SELECT g.id,g.name FROM genres g WHERE g.aggregated=0 AND g.id IN (SELECT genre_id FROM track_genres WHERE track_id IN (SELECT id FROM temp_tracks_list)) ORDER BY g.sort_order",metakeysQueried,library,db);
    }


    // Artists
    if(metakeysSelectedCopy.empty()){
        this->QueryForMetadata("artist","SELECT a.id,a.name FROM artists a WHERE a.aggregated=0 ORDER BY a.sort_order",metakeysQueried,library,db);
    }else{
        this->QueryForMetadata("artist","SELECT a.id,a.name FROM artists a WHERE a.aggregated=0 AND a.id IN (SELECT artist_id FROM track_artists WHERE track_id IN (SELECT id FROM temp_tracks_list)) ORDER BY a.sort_order",metakeysQueried,library,db);
    }

    ////////////////////////////////////////////////
    // Take care of selecting fixed fields like album,track,duration,year

    // Album
    if(metakeysSelectedCopy.empty()){
        this->QueryForMetadata("album","SELECT a.id,a.name FROM albums a ORDER BY a.sort_order",metakeysQueried,library,db);
    }else{
        this->QueryForMetadata("album","SELECT a.id,a.name FROM albums a WHERE a.id IN (SELECT album_id FROM temp_tracks_list) ORDER BY a.sort_order",metakeysQueried,library,db);
    }

    // Track
    if(metakeysSelectedCopy.empty()){
        this->QueryForMetadata("track","SELECT track,track FROM (SELECT DISTINCT(track) AS track FROM tracks ORDER BY track)",metakeysQueried,library,db);
    }else{
        this->QueryForMetadata("track","SELECT track,track FROM (SELECT DISTINCT(track) AS track FROM temp_tracks_list ORDER BY track)",metakeysQueried,library,db);
    }

    // Year
    if(metakeysSelectedCopy.empty()){
        this->QueryForMetadata("year","SELECT year,year FROM (SELECT DISTINCT(year) AS year FROM tracks ORDER BY year)",metakeysQueried,library,db);
    }else{
        this->QueryForMetadata("year","SELECT year,year FROM (SELECT DISTINCT(year) AS year FROM temp_tracks_list ORDER BY year)",metakeysQueried,library,db);
    }

    // Duration
    if(metakeysSelectedCopy.empty()){
        this->QueryForMetadata("duration","SELECT duration,duration FROM (SELECT DISTINCT(duration) AS duration FROM tracks ORDER BY duration)",metakeysQueried,library,db);
    }else{
        this->QueryForMetadata("duration","SELECT duration,duration FROM (SELECT DISTINCT(duration) AS duration FROM temp_tracks_list ORDER BY duration)",metakeysQueried,library,db);
    }


    ////////////////////////////////////////////////
    // Select metadata
    std::set<std::string> tempMetakeysQueried(metakeysQueried);

    {
        db::CachedStatement metakeyId("SELECT id FROM meta_keys WHERE name=?",db);

        for(std::set<std::string>::iterator metakey=tempMetakeysQueried.begin();metakey!=tempMetakeysQueried.end();++metakey){

            metakeyId.BindText(0,*metakey);

            if(metakeyId.Step()==db::Row){

                std::string metadataKeyId(metakeyId.ColumnText(0));
                std::string sql("SELECT mv.id,mv.content FROM meta_values mv WHERE mv.meta_key_id=");
                sql.append(metadataKeyId);
                if(metakeysSelectedCopy.empty()){
                    // List all
                    sql.append(" ORDER BY mv.sort_order");
                }else{
                    sql.append(" AND mv.id IN (SELECT meta_value_id FROM track_meta WHERE track_id IN (SELECT id FROM temp_tracks_list))");
                }
                this->QueryForMetadata(metakey->c_str(),sql.c_str(),metakeysQueried,library,db);

            }
            metakeyId.Reset();
        }
    }

    ////////////////////////////////////////////////
    // Select tracks
    std::string sql("SELECT t.id,t.duration,t.filesize FROM tracks t WHERE t.bpm>=");
	sql += boost::lexical_cast<std::string>(this->bpm);
	sql += " ORDER BY t.bpm ASC LIMIT 20";
    if(!metakeysSelectedCopy.empty()){
        sql    = "SELECT t.id,t.sum_duration,t.sum_filesize FROM temp_tracks_list t";
    }

    return (success && this->ParseTracksSQL(sql,library,db));
}

//////////////////////////////////////////
///\brief
///Copy a query
///
///\returns
///A shared_ptr to the Query::Base
//////////////////////////////////////////
Query::Ptr Query::BPMQuery::copy() const{
    Query::Ptr queryCopy(new Query::BPMQuery(*this));
    queryCopy->PostCopy();
    return queryCopy;
}

//////////////////////////////////////////
///\brief
///Receive the query from XML
///
///\param queryNode
///Reference to query XML node
///
///The excpeted input format is like this:
///\code
///<query type="ListSelection">
///   <selections>
///      <selection key="genre">1,3,5,7</selection>
///      <selection key="artist">6,7,8</selection>
///   </selections>
///   <listeners>genre,artist,album</listeners>
///</query>
///\endcode
///
///\returns
///true when successfully received
//////////////////////////////////////////
bool Query::BPMQuery::ReceiveQuery(musik::core::xml::ParserNode &queryNode){

    while( musik::core::xml::ParserNode node = queryNode.ChildNode() ){
        if(node.Name()=="selections"){
            // Get metakey nodes
            // Expected tag is likle this:
            // <selection key="genre">2,5,3</selection>
            while( musik::core::xml::ParserNode selectionNode = node.ChildNode("selection") ){

                // Wait for all content
                selectionNode.WaitForContent();

                // Split comaseparated list
                typedef std::vector<std::string> StringVector;
                StringVector values;
                boost::algorithm::split(values,selectionNode.Content(),boost::algorithm::is_any_of(","));

                for(StringVector::iterator value=values.begin();value!=values.end();++value){
                    this->SelectMetadata(selectionNode.Attributes()["key"].c_str(),boost::lexical_cast<DBINT>(*value));
                }

            }

		}else if(node.Name()=="bpm"){
			node.WaitForContent();
			this->bpm	= boost::lexical_cast<double>(node.Content());
			// TODO
        }else{
            this->ReceiveQueryStandardNodes(node);
        }
    }
    return true;
}

std::string Query::BPMQuery::Name(){
    return "BPMQuery";
}

///   <selections>
///      <selection key="genre">1,3,5,7</selection>
///      <selection key="artist">6,7,8</selection>
///   </selections>
bool Query::BPMQuery::SendQuery(musik::core::xml::WriterNode &queryNode){
	ListSelection::SendQuery(queryNode);

    xml::WriterNode bpmNode(queryNode,"bpm");
	bpmNode.Content() = boost::lexical_cast<std::string>(this->bpm);
    return true;
}

