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

#include <core/Common.h>
#include <core/library/query/TrackMetadataQuery.h>
#include <core/library/LibraryBase.h>
#include <core/LibraryTrack.h>
#include <boost/algorithm/string.hpp>

using namespace musik::core;
using namespace musik::core::query;

TrackMetadataQuery::TrackMetadataQuery(void) : requestAllFields(false){

}

TrackMetadataQuery::~TrackMetadataQuery(void){
}

void TrackMetadataQuery::RequestTrack(TrackPtr track){
    this->aRequestTracks.push_back(track);
}

void TrackMetadataQuery::Clear(){
    this->aRequestTracks.clear();
}

void TrackMetadataQuery::CreateSQL(){
    std::set<std::string> fields(this->requestedFields);
    std::set<std::string>::iterator field;

    this->sSQL          = "SELECT t.id";
    this->sSQLTables    = " FROM tracks t";
    this->sSQLWhere     = " WHERE t.id=? ";
    this->fieldOrder.clear();
    this->categoryFields.clear();

    //Lets find the fixed fields first
    this->GetFixedTrackMetakeys("track",fields);
    this->GetFixedTrackMetakeys("bpm",fields);
    this->GetFixedTrackMetakeys("duration",fields);
    this->GetFixedTrackMetakeys("filesize",fields);
    this->GetFixedTrackMetakeys("year",fields);
    this->GetFixedTrackMetakeys("title",fields);
    this->GetFixedTrackMetakeys("filename",fields);
    this->GetFixedTrackMetakeys("thumbnail_id",fields);

    if( (field=fields.find("path"))!=fields.end() ){
        this->sSQL          += ",p.path||f.relative_path||'/'||t.filename";
        this->sSQLTables    += ",folders f,paths p";
        this->sSQLWhere     += " AND t.folder_id=f.id AND f.path_id=p.id";
        this->fieldOrder.push_back(*field);
        fields.erase(field);
    }
    if( (field=fields.find("visual_genre"))!=fields.end() ){
        this->sSQL          += ",g.name";
        this->sSQLTables    += ",genres g";
        this->sSQLWhere     += " AND t.visual_genre_id=g.id";
        this->fieldOrder.push_back(*field);
        fields.erase(field);
    }
    if( (field=fields.find("visual_artist"))!=fields.end() ){
        this->sSQL          += ",ar.name";
        this->sSQLTables    += ",artists ar";
        this->sSQLWhere     += " AND t.visual_artist_id=ar.id";
        this->fieldOrder.push_back(*field);
        fields.erase(field);
    }
    if( (field=fields.find("album"))!=fields.end() ){
        this->sSQL          += ",al.name";
        this->sSQLTables    += ",albums al";
        this->sSQLWhere     += " AND t.album_id=al.id";
        this->fieldOrder.push_back(*field);
        fields.erase(field);
    }

    // Fix category fields
    if( (field=fields.find("artist"))!=fields.end() ){
        this->categoryFields.insert(*field);
        fields.erase(field);
    }
    if( (field=fields.find("genre"))!=fields.end() ){
        this->categoryFields.insert(*field);
        fields.erase(field);
    }


    // set the rest as metafields
    this->metaFields.swap(fields);

}

void TrackMetadataQuery::GetFixedTrackMetakeys(std::string fieldName,std::set<std::string> &fields){
    std::set<std::string>::iterator field;
    if( (field=fields.find(fieldName))!=fields.end() ){
        this->sSQL    += ",t."+fieldName;
        this->fieldOrder.push_back(fieldName);
        fields.erase(field);
    }
}


bool TrackMetadataQuery::ParseQuery(library::LibraryBase *library,db::Connection &db){

    db::CachedStatement genres("SELECT g.name FROM genres g,track_genres tg WHERE tg.genre_id=g.id AND tg.track_id=? ORDER BY tg.id",db);
    db::CachedStatement artists("SELECT ar.name FROM artists ar,track_artists ta WHERE ta.artist_id=ar.id AND ta.track_id=? ORDER BY ta.id",db);
    db::CachedStatement metadata("SELECT mv.content FROM meta_values mv,track_meta tm WHERE tm.track_id=? AND tm.meta_value_id=mv.id AND mv.meta_key_id=(SELECT id FROM meta_keys WHERE name=?) ORDER BY tm.id",db);
    db::CachedStatement allMetadata("SELECT mv.content,mk.name FROM meta_values mv,meta_keys mk,track_meta tm WHERE tm.track_id=? AND tm.meta_value_id=mv.id AND mv.meta_key_id=mk.id ORDER BY tm.id",db);


    std::string sql=this->sSQL+this->sSQLTables+this->sSQLWhere;

    if(sql.empty()){
        // ERROR.. SOMETHING IS WRONG HERE
        return true;
    }

    db::CachedStatement trackData(sql.c_str(),db);

    bool bCancel(false);

    while(!this->aRequestTracks.empty() && !library->QueryCanceled(this)){
        TrackPtr track(this->aRequestTracks.back());
        this->aRequestTracks.pop_back();

        trackData.BindInt(0,track->Id());

        if(trackData.Step()==db::Row){

            // fetch the result
            std::vector<std::string>::iterator field=this->fieldOrder.begin();
            int dbField(1);    // one ahead to ignore the id
            while(field!=this->fieldOrder.end()){
                const char *valuePointer=trackData.ColumnText(dbField);
                if(valuePointer){
                    track->SetValue(field->c_str(),valuePointer);
                }
                ++field;
                ++dbField;
            }

            // Get the meta-value as well
            if(this->requestAllFields){
                // Get ALL meta
                allMetadata.BindInt(0,track->Id());
                while(allMetadata.Step()==db::Row){
                    track->SetValue(allMetadata.ColumnText(1),allMetadata.ColumnText(0));
                }
                allMetadata.Reset();

            }else{
                for(std::set<std::string>::iterator metaKey=this->metaFields.begin();metaKey!=this->metaFields.end();++metaKey){

                    metadata.BindInt(0,track->Id());
                    metadata.BindText(1,metaKey->c_str());

                    while(metadata.Step()==db::Row){
                        track->SetValue(metaKey->c_str(),metadata.ColumnText(0));
                    }
                    metadata.Reset();
                }
            }


            // Find genres
            if( this->categoryFields.find("genre")!=this->categoryFields.end() ){
                genres.BindInt(0,track->Id());
                while(genres.Step()==db::Row){
                    track->SetValue("genre",genres.ColumnText(0));
                }
                genres.Reset();
            }

            // Find artists
            if( this->categoryFields.find("artist")!=this->categoryFields.end() ){
                artists.BindInt(0,track->Id());
                while(artists.Step()==db::Row){
                    track->SetValue("artist",artists.ColumnText(0));
                }
                artists.Reset();
            }


            {
                boost::mutex::scoped_lock oLock(library->resultMutex);
                this->aResultTracks.push_back(track);
            }
        }

        trackData.Reset();

    }


    {
        boost::mutex::scoped_lock lock(library->libraryMutex);
        this->status |= QueryBase::Ended;
    }

    return true;
}

bool TrackMetadataQuery::RunCallbacks(library::LibraryBase *library){

    TrackVector aResultCopy;
    bool bReturn(false);

    // First swap the results so that Query can continue to parse
    {
        boost::mutex::scoped_lock lock(library->resultMutex);
        aResultCopy.swap(this->aResultTracks);
    }
    {
        boost::mutex::scoped_lock lock(library->libraryMutex);
        if( (this->status & QueryBase::Ended)!=0){
            bReturn    = true;
        }

    }

    if(!aResultCopy.empty()){
        // Call callbacks
        this->OnTracksEvent(&aResultCopy);
    }

    return bReturn;
}

void TrackMetadataQuery::RequestMetakeys(const std::set<std::string> &fields){
    this->requestAllFields      = false;
    this->requestedFields       = fields;
    this->CreateSQL();
}

void TrackMetadataQuery::RequestAllMetakeys(){
    this->requestAllFields    = true;

    this->requestedFields.insert("track");
    this->requestedFields.insert("bpm");
    this->requestedFields.insert("duration");
    this->requestedFields.insert("filesize");
    this->requestedFields.insert("year");
    this->requestedFields.insert("title");
    this->requestedFields.insert("filename");
    this->requestedFields.insert("path");
    this->requestedFields.insert("visual_genre");
    this->requestedFields.insert("visual_artist");
    this->requestedFields.insert("album");
    this->requestedFields.insert("genre");
    this->requestedFields.insert("artist");
    this->requestedFields.insert("thumbnail_id");

    this->CreateSQL();

}

query::Ptr TrackMetadataQuery::copy() const{
    query::Ptr queryCopy(new query::TrackMetadataQuery(*this));
    queryCopy->PostCopy();
    return queryCopy;
}

void TrackMetadataQuery::PreAddQuery(library::LibraryBase *library){
/*    for(TrackVector::iterator track=this->aRequestTracks.begin();track!=this->aRequestTracks.end();++track){
        (*track)->InitMeta(library);
    }*/
}

std::string TrackMetadataQuery::Name(){
    return "TrackMetadataQuery";
}
