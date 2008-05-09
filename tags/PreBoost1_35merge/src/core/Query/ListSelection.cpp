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
#include <core/Query/ListSelection.h>
#include <core/Library/Base.h>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <algorithm>

using namespace musik::core;


//////////////////////////////////////////
///\brief
///Constructor
//////////////////////////////////////////
Query::ListSelection::ListSelection(void) : selectionOrderSensitive(true){
}

//////////////////////////////////////////
///\brief
///Destructor
//////////////////////////////////////////
Query::ListSelection::~ListSelection(void){
}

//////////////////////////////////////////
///\brief
///Set if the list should be sensitive for the selection order of metalist. Default is true.
///
///\param sensitive
///sensitive or not
///
///The behavior of the query will be a lot different if this is turned on.
///Consider the folloing example:
///\code
/// ...
/// musik::core::Query::ListSelection query;
/// ... // Connect the "artist" and "album" using the OnMetadataEvent()
///
/// query.SelectMetadata("genre",1); // selects the genre with id 1
/// library.AddQuery(query);            // this will list "artist" and "album"
///
/// query.SelectMetadata("artist",3); // select the artist with id 3
///
/// // this is where the selection order differes.
/// query.SelectMetadata("genre",2); // Adds the genre id 2 to the selection
/// // If the Sensitivity is turned on, the selection of "artist" will be cleared
/// // witch means that the query will automatically call query.ClearMetadata("artist").
///\endcode
///
//////////////////////////////////////////
void Query::ListSelection::SelectionOrderSensitive(bool sensitive){
    this->selectionOrderSensitive    = sensitive;
}


//////////////////////////////////////////
///\brief
///Select a specific metadata
///
///\param metakey
///meta key to select. Like "genre" or "artist"
///
///\param metadataId
///The database identifier of the metakeys value
///
///\remarks
///SelectMetadata will add to the selection.
///
///\see
///ClearMetadata|RemoveMetadata
//////////////////////////////////////////
void Query::ListSelection::SelectMetadata(const char* metakey,DBINT metadataId){

    if(this->selectionOrderSensitive){
        std::vector<std::string>::iterator    themetakey    = std::find(this->metakeySelectionOrder.begin(),this->metakeySelectionOrder.end(),metakey);
        if(themetakey==this->metakeySelectionOrder.end()){
            // Not in selection yet
            this->metakeySelectionOrder.push_back(metakey);
        }else{

            // Erase everything after this metakey
            ++themetakey;
            if( themetakey!=this->metakeySelectionOrder.end() ){
                // Clear selection
                this->ClearMetadata(themetakey->c_str());
            }
        }
    }

    this->selectedMetadata[metakey].insert(metadataId);
}

//////////////////////////////////////////
///\brief
///Remove a selection
///
///\param metakey
///metakey to remove. Like "genre" or "artist"
///
///\param metadataId
///The database identifier of the metakeys value
///
///\see
///ClearMetadata|SelectMetadata
//////////////////////////////////////////
void Query::ListSelection::RemoveMetadata(const char* metatag,DBINT metadataId){
    SelectedMetadata::iterator    keyiterator    = this->selectedMetadata.find(metatag);
    if(keyiterator!=this->selectedMetadata.end()){
        keyiterator->second.erase(metadataId);
        if(keyiterator->second.empty()){
            // All selections have been removed, Clear
            this->ClearMetadata(metatag);
        }else{
            if(this->selectionOrderSensitive){
                std::vector<std::string>::iterator    themetakey    = std::find(this->metakeySelectionOrder.begin(),this->metakeySelectionOrder.end(),metatag);
                if(themetakey==this->metakeySelectionOrder.end()){
                    ++themetakey;
                    if( themetakey!=this->metakeySelectionOrder.end() ){
                        // Clear selection
                        this->ClearMetadata(themetakey->c_str());
                    }
                }
            }
        }
    }
}

//////////////////////////////////////////
///\brief
///Clear a selection totaly or clear a specific metakey selection
///
///\param metakey
///metakey to clear. Like "genre" or "artist"
///
///\see
///RemoveMetadata|SelectMetadata
//////////////////////////////////////////
void Query::ListSelection::ClearMetadata(const char* metatag){
    if(metatag==NULL){
        this->selectedMetadata.clear();
        this->metakeySelectionOrder.clear();
    }else{
        this->selectedMetadata.erase(metatag);

        if(this->selectionOrderSensitive){
            std::vector<std::string>::iterator    metakey    = std::find(this->metakeySelectionOrder.begin(),this->metakeySelectionOrder.end(),metatag);
            while( metakey!=this->metakeySelectionOrder.end() ){
                // Clear selection
                this->selectedMetadata.erase( *metakey );
                // Go to the next selection
                metakey    = this->metakeySelectionOrder.erase(metakey);
            }
        }

    }
}


//////////////////////////////////////////
///\brief
///Parse the list query
///
///\param oLibrary
///pointer to library that parses the query. Needed for mutexes.
///
///\param db
///pointer to database connect object
///
///\returns
///true when successfully parsed
///
///This is a pretty tricky thing to parse.
///First lets select the tracks
///        SELECT t.id FROM tracks t ORDER BY t.sort_order1
///
///        1. lets check the genre and artists metadata
///            if they are selected insert into the select like:
///                SELECT t.id FROM tracks t WHERE t.id IN (SELECT track_id FROM track_genres WHERE genre_id IN ($genrelist$))
///                    AND t.id IN (SELECT track_id FROM track_artists WHERE artist_id IN ($artistlist$))
///            this should make SQLite use the indexes on the track_* tables.
///
///        2. Check the album and the fixed fields
///            SELECT t.id FROM tracks t WHERE album_id IN ($albumlist$)
///                AND year IN ($yearlist$)
///            This will only work on fields that are INTEGERS
///                year,duration,filesize,track,album_id
///
///        3. Check for other selected metadata
///            SELECT t.id FROM tracks t WHERE t.id IN
///                (SELECT track_id FROM track_meta WHERE meta_value_id IN ($metadatalist$))
///
///    Second, we need to select the metadata
///
///        1. if genre or artists are connected for callbacks
///            SELECT g.id,g.name FROM genres g WHERE g.id IN (SELECT genre_id FROM track_genres WHERE track_id IN ($tracklist$)) ORDER BY g.sort_order
///
///            I guess I could generate a list from the tracks, but this list
///            could become VERY long (20000 tracks is not uncommon). Using the
///            sql from the tracks select is my temporary solution.
///
///        2. Getting fixed fields
///            Well.. I guess I could do this at the same time when I select the tracks by adding them in the select
///                SELECT t.id,t.year FROM tracks t ORDER BY t.sort_order1
///            and store the output in a std::set<DBINT> to get the uniq fields
///            This way they will also be sorted.
///            Do the same for the "album" but get album_id and query for it later.
///
///        3. Get the metadata
///            SELECT id,content FROM meta_values WHERE meta_key_id IN (SELECT id FROM meta_keys WHERE name=?) AND id IN (SELECT meta_value_id FROM track_meta WHERE track_id IN ($tracklist$)) ORDER BY sort_order
///
//////////////////////////////////////////
bool Query::ListSelection::ParseQuery(Library::Base *oLibrary,db::Connection &db){

    bool success(true);

    ////////////////////////////////////////////////
    // First lets make sure we do not have any signals
    // or selections that are empty
    for(MetadataSignals::iterator signal=this->metadataEvent.begin();signal!=this->metadataEvent.end();){
        if( !signal->second.has_connections() ){
            signal    = this->metadataEvent.erase(signal);
        }else{
            ++signal;
        }
    }

    for(SelectedMetadata::iterator selected=this->selectedMetadata.begin();selected!=this->selectedMetadata.end();){
        if( selected->second.empty() ){
            selected    = this->selectedMetadata.erase(selected);
        }else{
            ++selected;
        }
    }

    ////////////////////////////////////////////////
    // Second. Select track
    std::string sqlSelectTrack("SELECT t.id AS id");
    std::string sqlSelectTrackFrom(" FROM tracks t ");
    std::string sqlSelectTrackWhere;
    std::string sqlSelectTrackOrder("ORDER BY t.sort_order1");

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
        this->SQLSelectQuery("genre","t.id IN (SELECT track_id FROM track_genres WHERE genre_id IN (",")) ",metakeysSelected,sqlSelectTrackWhere,oLibrary);

        ////////////////////////////////////////////////
        // Selected artists
        this->SQLSelectQuery("artist","t.id IN (SELECT track_id FROM track_artists WHERE artist_id IN (",")) ",metakeysSelected,sqlSelectTrackWhere,oLibrary);

        ////////////////////////////////////////////////
        // Selected albums
        this->SQLSelectQuery("album","t.album_id IN (",") ",metakeysSelected,sqlSelectTrackWhere,oLibrary);

        ////////////////////////////////////////////////
        // Selected year
        this->SQLSelectQuery("year","t.year IN (",") ",metakeysSelected,sqlSelectTrackWhere,oLibrary);

        ////////////////////////////////////////////////
        // Selected duration
        this->SQLSelectQuery("duration","t.duration IN (",") ",metakeysSelected,sqlSelectTrackWhere,oLibrary);

        ////////////////////////////////////////////////
        // Selected track
        this->SQLSelectQuery("track","t.track IN (",") ",metakeysSelected,sqlSelectTrackWhere,oLibrary);

        ////////////////////////////////////////////////
        // Selected metadata
        std::set<std::string> tempMetakeysSelected(metakeysSelected);
        for(std::set<std::string>::iterator metakey=tempMetakeysSelected.begin();metakey!=tempMetakeysSelected.end();++metakey){
            this->SQLSelectQuery(metakey->c_str(),"t.id IN (SELECT track_id FROM track_meta WHERE meta_value_id IN (",")) ",metakeysSelected,sqlSelectTrackWhere,oLibrary);
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
        this->QueryForMetadata("genre","SELECT g.id,g.name FROM genres g WHERE g.aggregated=0 ORDER BY g.sort_order",metakeysQueried,oLibrary,db);
    }else{
        this->QueryForMetadata("genre","SELECT g.id,g.name FROM genres g WHERE g.aggregated=0 AND g.id IN (SELECT genre_id FROM track_genres WHERE track_id IN (SELECT id FROM temp_tracks_list)) ORDER BY g.sort_order",metakeysQueried,oLibrary,db);
    }


    // Artists
    if(metakeysSelectedCopy.empty()){
        this->QueryForMetadata("artist","SELECT a.id,a.name FROM artists a WHERE a.aggregated=0 ORDER BY a.sort_order",metakeysQueried,oLibrary,db);
    }else{
        this->QueryForMetadata("artist","SELECT a.id,a.name FROM artists a WHERE a.aggregated=0 AND a.id IN (SELECT artist_id FROM track_artists WHERE track_id IN (SELECT id FROM temp_tracks_list)) ORDER BY a.sort_order",metakeysQueried,oLibrary,db);
    }

    ////////////////////////////////////////////////
    // Take care of selecting fixed fields like album,track,duration,year

    // Album
    if(metakeysSelectedCopy.empty()){
        this->QueryForMetadata("album","SELECT a.id,a.name FROM albums a ORDER BY a.sort_order",metakeysQueried,oLibrary,db);
    }else{
        this->QueryForMetadata("album","SELECT a.id,a.name FROM albums a WHERE a.id IN (SELECT album_id FROM temp_tracks_list) ORDER BY a.sort_order",metakeysQueried,oLibrary,db);
    }

    // Track
    if(metakeysSelectedCopy.empty()){
        this->QueryForMetadata("track","SELECT track,track FROM (SELECT DISTINCT(track) AS track FROM tracks ORDER BY track)",metakeysQueried,oLibrary,db);
    }else{
        this->QueryForMetadata("track","SELECT track,track FROM (SELECT DISTINCT(track) AS track FROM temp_tracks_list ORDER BY track)",metakeysQueried,oLibrary,db);
    }

    // Year
    if(metakeysSelectedCopy.empty()){
        this->QueryForMetadata("year","SELECT year,year FROM (SELECT DISTINCT(year) AS year FROM tracks ORDER BY year)",metakeysQueried,oLibrary,db);
    }else{
        this->QueryForMetadata("year","SELECT year,year FROM (SELECT DISTINCT(year) AS year FROM temp_tracks_list ORDER BY year)",metakeysQueried,oLibrary,db);
    }

    // Duration
    if(metakeysSelectedCopy.empty()){
        this->QueryForMetadata("duration","SELECT duration,duration FROM (SELECT DISTINCT(duration) AS duration FROM tracks ORDER BY duration)",metakeysQueried,oLibrary,db);
    }else{
        this->QueryForMetadata("duration","SELECT duration,duration FROM (SELECT DISTINCT(duration) AS duration FROM temp_tracks_list ORDER BY duration)",metakeysQueried,oLibrary,db);
    }


    ////////////////////////////////////////////////
    // Select metadata
    std::set<std::string> tempMetakeysQueried(metakeysQueried);

    {
        db::CachedStatement metakeyId("SELECT id FROM meta_keys WHERE name=?",db);

        for(std::set<std::string>::iterator metakey=tempMetakeysQueried.begin();metakey!=tempMetakeysQueried.end();++metakey){

            metakeyId.BindText(0,*metakey);

            if(metakeyId.Step()==db::ReturnCode::Row){

                std::string metadataKeyId(metakeyId.ColumnText(0));
                std::string sql("SELECT mv.id,mv.content FROM meta_values mv WHERE mv.meta_key_id=");
                sql.append(metadataKeyId);
                if(metakeysSelectedCopy.empty()){
                    // List all
                    sql.append(" ORDER BY mv.sort_order");
                }else{
                    sql.append(" AND mv.id IN (SELECT meta_value_id FROM track_meta WHERE track_id IN (SELECT id FROM temp_tracks_list))");
                }
                this->QueryForMetadata(metakey->c_str(),sql.c_str(),metakeysQueried,oLibrary,db);

            }
            metakeyId.Reset();
        }
    }

    if(this->trackEvent.has_connections() && !oLibrary->QueryCanceled()){

        std::string sql("SELECT t.id FROM tracks t ORDER BY t.sort_order1");
        if(!metakeysSelectedCopy.empty()){
            sql    = "SELECT t.id FROM temp_tracks_list t";
        }
        db::CachedStatement tracks(sql.c_str(),db);


        // For optimization, lets just add to results every 100 track
        TrackVector tempTrackResults;
        tempTrackResults.reserve(101);
        int row(0);

        while(tracks.Step()==db::ReturnCode::Row){
            tempTrackResults.push_back(TrackPtr(new Track(tracks.ColumnInt(0))));
            if( (++row)%100==0 ){
                boost::mutex::scoped_lock lock(oLibrary->oResultMutex);
                this->trackResults.insert(this->trackResults.end(),tempTrackResults.begin(),tempTrackResults.end());
                tempTrackResults.clear();
                trackResults.reserve(101);
            }
        }
        if(!tempTrackResults.empty()){
            boost::mutex::scoped_lock lock(oLibrary->oResultMutex);
            this->trackResults.insert(this->trackResults.end(),tempTrackResults.begin(),tempTrackResults.end());
        }

    }

    db.Execute("DROP TABLE IF EXISTS temp_tracks_list");

    return success;
}

//////////////////////////////////////////
///\brief
///Copy a query
///
///\returns
///A shared_ptr to the Query::Base
//////////////////////////////////////////
Query::Ptr Query::ListSelection::copy() const{
    return Query::Ptr(new Query::ListSelection(*this));
}

void Query::ListSelection::SQLPrependWhereOrAnd(std::string &sql){
    if(sql.empty()){
        sql.append("WHERE ");
    }else{
        sql.append("AND ");
    }
}

//////////////////////////////////////////
///\brief
///Helper method to construct SQL query for selected metakeys
//////////////////////////////////////////
void Query::ListSelection::SQLSelectQuery(const char *metakey,const char *sqlStart,const char *sqlEnd,std::set<std::string> &metakeysSelected,std::string &sqlSelectTrackWhere,Library::Base *oLibrary){

    if(!oLibrary->QueryCanceled()){
        SelectedMetadata::iterator selected    = this->selectedMetadata.find(metakey);
        if(selected!=this->selectedMetadata.end()){

            this->SQLPrependWhereOrAnd(sqlSelectTrackWhere);
            sqlSelectTrackWhere.append(sqlStart);

            bool first(true);
            for(std::set<DBINT>::iterator id=selected->second.begin();id!=selected->second.end();++id){
                if(first){
                    first=false;
                }else{
                    sqlSelectTrackWhere.append(",");
                }
                sqlSelectTrackWhere.append(boost::lexical_cast<std::string>(*id));
            }
            sqlSelectTrackWhere.append(sqlEnd);
        }

        metakeysSelected.erase(metakey);
    }
}

//////////////////////////////////////////
///\brief
///Method called by ParseQuery for every queried metakey
//////////////////////////////////////////
void Query::ListSelection::QueryForMetadata(const char *metakey,const char *sql,std::set<std::string> &metakeysQueried,Library::Base *oLibrary,db::Connection &db){
    if(oLibrary->QueryCanceled())
        return;

    if(metakeysQueried.find(metakey)!=metakeysQueried.end()){

        db::Statement metaValueStmt(sql,db);

        MetadataValueVector tempMetadataValues;
        tempMetadataValues.reserve(10);
        int row(0);

        {
            boost::mutex::scoped_lock lock(oLibrary->oResultMutex);
            this->metadataResults[metakey];
        }

        while(metaValueStmt.Step()==db::ReturnCode::Row){
            tempMetadataValues.push_back(
                    MetadataValuePtr(
                        new MetadataValue(
                            metaValueStmt.ColumnInt(0),
                            metaValueStmt.ColumnTextUTF(1)
                            )
                        )
                    );

            if( (++row)%10==0 ){
                boost::mutex::scoped_lock lock(oLibrary->oResultMutex);
                this->metadataResults[metakey].insert(this->metadataResults[metakey].end(),tempMetadataValues.begin(),tempMetadataValues.end());
                tempMetadataValues.clear();
                tempMetadataValues.reserve(10);
            }
        }
        if(!tempMetadataValues.empty()){
            boost::mutex::scoped_lock lock(oLibrary->oResultMutex);
            this->metadataResults[metakey].insert(this->metadataResults[metakey].end(),tempMetadataValues.begin(),tempMetadataValues.end());
        }

        metakeysQueried.erase(metakey);
    }
}




