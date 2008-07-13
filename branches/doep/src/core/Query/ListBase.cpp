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
#include <core/Query/ListBase.h>
#include <core/Library/Base.h>
#include <core/Common.h>

using namespace musik::core;


Query::ListBase::ListBase(void) 
:clearedTrackResults(false)
,trackInfoTracks(0)
,trackInfoDuration(0)
,trackInfoSize(0)
{
}

Query::ListBase::~ListBase(void){
}


bool Query::ListBase::RunCallbacks(Library::Base *library){

    bool bReturn(false);

    MetadataResults metadataResultsCopy;
    TrackVector trackResultsCopy;

    {    // Scope for swapping the results safely
        boost::mutex::scoped_lock lock(library->oResultMutex);

        metadataResultsCopy.swap(this->metadataResults);
        trackResultsCopy.swap(this->trackResults);

        if( (this->status & Status::Ended)!=0){
            // If the query is finished, this function should return true to report that it is finished.
            bReturn    = true;
        }
    }

    // Check for metadata results
    if(metadataResultsCopy.size()!=0){

        // Loop metadata tags for results
        for( MetadataResults::iterator metatagResult=metadataResultsCopy.begin();metatagResult!=metadataResultsCopy.end();++metatagResult){

            std::string metatag(metatagResult->first);

            // If the metatag  has results, call the signals
//            if( !metatagResult->second.empty() ){

                // check if the signal should send the clear flag
                bool clearMetatag(false);
                if(this->clearedMetadataResults.find(metatag)==this->clearedMetadataResults.end()){
                    clearMetatag    = true;
                    this->clearedMetadataResults.insert(metatag);    // Set this to cleared
                }

                // Call the signal
                this->metadataEvent[metatag](&metatagResult->second,clearMetatag);
//            }
        }
    }

    // Check for Tracks
    if( !trackResultsCopy.empty() ){

        // Call the slots
        this->trackEvent(&trackResultsCopy,!this->clearedTrackResults);

        if(!this->clearedTrackResults){
            this->clearedTrackResults    = true;
        }

    }

    if(bReturn){
        boost::mutex::scoped_lock lock(library->oResultMutex);
        // Check for trackinfo update
        this->trackInfoEvent(trackInfoTracks,trackInfoDuration,trackInfoSize);
    }

    return bReturn;
}

Query::ListBase::MetadataSignal& Query::ListBase::OnMetadataEvent(const char* metatag){
    return this->metadataEvent[metatag];
}

Query::ListBase::MetadataSignal& Query::ListBase::OnMetadataEvent(const wchar_t* metatag){
    return this->metadataEvent[ConvertUTF8(std::wstring(metatag))];
}

Query::ListBase::TrackSignal& Query::ListBase::OnTrackEvent(){
    return this->trackEvent;
}

Query::ListBase::TrackInfoSignal& Query::ListBase::OnTrackInfoEvent(){
    return this->trackInfoEvent;
}

bool Query::ListBase::ParseTracksSQL(std::string sql,Library::Base *library,db::Connection &db){
    if(this->trackEvent.has_connections() && !library->QueryCanceled()){
        db::Statement selectTracks(sql.c_str(),db);

        TrackVector tempTrackResults;
        tempTrackResults.reserve(101);
        int row(0);
        while(selectTracks.Step()==db::ReturnCode::Row){
            tempTrackResults.push_back(TrackPtr(new Track(selectTracks.ColumnInt(0))));
            this->trackInfoDuration += selectTracks.ColumnInt64(1);
            this->trackInfoSize     += selectTracks.ColumnInt64(2);
            this->trackInfoTracks++;

            if( (++row)%100==0 ){
                boost::mutex::scoped_lock lock(library->oResultMutex);
                this->trackResults.insert(this->trackResults.end(),tempTrackResults.begin(),tempTrackResults.end());

                tempTrackResults.clear();
                trackResults.reserve(101);
            }
        }
        if(!tempTrackResults.empty()){
            boost::mutex::scoped_lock lock(library->oResultMutex);
            this->trackResults.insert(this->trackResults.end(),tempTrackResults.begin(),tempTrackResults.end());
        }

        return true;
    }else{
        return false;
    }

}

bool Query::ListBase::SendResults(musik::core::xml::WriterNode &queryNode,Library::Base *library){

    bool continueSending(true);
    while(continueSending){

        MetadataResults metadataResultsCopy;
        TrackVector trackResultsCopy;

        {    // Scope for swapping the results safely
            boost::mutex::scoped_lock lock(library->oResultMutex);

            metadataResultsCopy.swap(this->metadataResults);
            trackResultsCopy.swap(this->trackResults);

            if( (this->status & Status::Ended)!=0){
                // If the query is finished, stop sending
                continueSending = false;
            }
        }

        // Check for metadata results
        if(!metadataResultsCopy.empty()){

            // Loop metadata tags for results
            for( MetadataResults::iterator metatagResult=metadataResultsCopy.begin();metatagResult!=metadataResultsCopy.end();++metatagResult){

                std::string metatag(metatagResult->first);

                // If the metatag  has results, send results

                // check if the signal should send the clear flag
                bool clearMetatag(false);
                if(this->clearedMetadataResults.find(metatag)==this->clearedMetadataResults.end()){
                    clearMetatag    = true;
                    this->clearedMetadataResults.insert(metatag);    // Set this to cleared
                }

                // Send results
                {
                    musik::core::xml::WriterNode results(queryNode,"metadata");

                    results.Attributes()["key"] = metatagResult->first;
                    if(clearMetatag){
                        results.Attributes()["clear"] = "true";
                    }

                    for(musik::core::MetadataValueVector::iterator metaValue=metatagResult->second.begin();metaValue!=metatagResult->second.end();++metaValue){
                        musik::core::xml::WriterNode metaValueNode(results,"md");
                        metaValueNode.Attributes()["id"]    = boost::lexical_cast<std::string>( (*metaValue)->id );

                        metaValueNode.Content() = musik::core::ConvertUTF8( (*metaValue)->value );

                    }

                    this->metadataEvent[metatag](&metatagResult->second,clearMetatag);
                }
            }
        }

        // Check for Tracks
        if( !trackResultsCopy.empty() ){

            musik::core::xml::WriterNode tracklist(queryNode,"tracklist");

            if(!this->clearedTrackResults){
                tracklist.Attributes()["clear"]    = "true";
                this->clearedTrackResults    = true;
            }

            for(musik::core::TrackVector::iterator track=trackResultsCopy.begin();track!=trackResultsCopy.end();){
                musik::core::xml::WriterNode tracks(tracklist,"tracks");
                int trackCount(0);
                while(trackCount<100 && track!=trackResultsCopy.end()){
                    if(trackCount!=0){
                        tracks.Content()    += ",";             
                    }
                    tracks.Content()    += boost::lexical_cast<std::string>( (*track)->id );   
                    ++track;
                    ++trackCount;
                }
            }

        }

        if(!continueSending){
            boost::mutex::scoped_lock lock(library->oResultMutex);
            // Check for trackinfo update
            musik::core::xml::WriterNode trackInfoNode(queryNode,"trackinfo");
            trackInfoNode.Content() =  boost::lexical_cast<std::string>( trackInfoTracks );
            trackInfoNode.Content() += ","+boost::lexical_cast<std::string>( trackInfoDuration );
            trackInfoNode.Content() += ","+boost::lexical_cast<std::string>( trackInfoSize );
        }else{
            if( metadataResultsCopy.empty() && trackResultsCopy.empty() ){
                // Yield for more results
                boost::thread::yield();
            }
        }
    }

    return true;
}

void Query::ListBase::DummySlot(MetadataValueVector*,bool){
}

void Query::ListBase::DummySlotTracks(TrackVector*,bool){
}

void Query::ListBase::DummySlotTrackInfo(UINT64,UINT64,UINT64){
}



