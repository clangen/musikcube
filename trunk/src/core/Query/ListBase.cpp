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
#include <core/xml/ParserNode.h>
#include <core/xml/WriterNode.h>
#include <core/LibraryTrack.h>
#include <boost/algorithm/string.hpp>

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
        boost::mutex::scoped_lock lock(library->resultMutex);

        metadataResultsCopy.swap(this->metadataResults);
        trackResultsCopy.swap(this->trackResults);

    }

    {
        boost::mutex::scoped_lock lock(library->libraryMutex);
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
        boost::mutex::scoped_lock lock(library->resultMutex);
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
    if(this->trackEvent.has_connections() && !library->QueryCanceled(this)){
        db::Statement selectTracks(sql.c_str(),db);

        TrackVector tempTrackResults;
        tempTrackResults.reserve(101);
        int row(0);
        while(selectTracks.Step()==db::ReturnCode::Row){
            tempTrackResults.push_back(TrackPtr(new LibraryTrack(selectTracks.ColumnInt(0),library->Id())));
            this->trackInfoDuration += selectTracks.ColumnInt64(1);
            this->trackInfoSize     += selectTracks.ColumnInt64(2);
            this->trackInfoTracks++;

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

bool Query::ListBase::SendResults(musik::core::xml::WriterNode &queryNode,Library::Base *library){

    bool continueSending(true);
    while(continueSending && !library->QueryCanceled(this)){

        MetadataResults metadataResultsCopy;
        TrackVector trackResultsCopy;

        {    // Scope for swapping the results safely
            boost::mutex::scoped_lock lock(library->resultMutex);

            metadataResultsCopy.swap(this->metadataResults);
            trackResultsCopy.swap(this->trackResults);
        }
        {
            boost::mutex::scoped_lock lock(library->libraryMutex);

            if( (this->status & Status::Ended)!=0){
                // If the query is finished, stop sending
                continueSending = false;
            }
        }

        // Check for metadata results
        if(!metadataResultsCopy.empty()){

            // Loop metadata tags for results
            for( MetadataResults::iterator metatagResult=metadataResultsCopy.begin();metatagResult!=metadataResultsCopy.end() && !library->QueryCanceled(this);++metatagResult){

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
        if( !trackResultsCopy.empty() && !library->QueryCanceled(this) ){

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
                    tracks.Content()    += boost::lexical_cast<std::string>( (*track)->Id() );   
                    ++track;
                    ++trackCount;
                }
            }

        }

        if(continueSending){
            if( metadataResultsCopy.empty() && trackResultsCopy.empty() ){
                // Yield for more results
                boost::thread::yield();
            }
        }
    }
	
	if(!library->QueryCanceled(this)){
        boost::mutex::scoped_lock lock(library->resultMutex);
        // Check for trackinfo update
        musik::core::xml::WriterNode trackInfoNode(queryNode,"trackinfo");
        trackInfoNode.Content() =  boost::lexical_cast<std::string>( this->trackInfoTracks );
        trackInfoNode.Content() += ","+boost::lexical_cast<std::string>( this->trackInfoDuration );
        trackInfoNode.Content() += ","+boost::lexical_cast<std::string>( this->trackInfoSize );
	}


    return true;
}

bool Query::ListBase::RecieveResults(musik::core::xml::ParserNode &queryNode,Library::Base *library){
    while( musik::core::xml::ParserNode node = queryNode.ChildNode() ){

		// Recieve metadata
        if( node.Name()=="metadata"){

            std::string metakey(node.Attributes()["key"]);

            MetadataValueVector tempMetadataValues;
            tempMetadataValues.reserve(10);
            int row(0);

            {
                boost::mutex::scoped_lock lock(library->resultMutex);
                this->metadataResults[metakey];
            }

            while( musik::core::xml::ParserNode metaDataNode = node.ChildNode("md") ){
                metaDataNode.WaitForContent();

                tempMetadataValues.push_back(
                        MetadataValuePtr(
                            new MetadataValue(
                                boost::lexical_cast<unsigned int>( metaDataNode.Attributes()["id"] ),
                                ConvertUTF16(metaDataNode.Content()).c_str()
                                )
                            )
                        );

                if( (++row)%10==0 ){
                    boost::mutex::scoped_lock lock(library->resultMutex);
                    this->metadataResults[metakey].insert(this->metadataResults[metakey].end(),tempMetadataValues.begin(),tempMetadataValues.end());
                    tempMetadataValues.clear();
                    tempMetadataValues.reserve(10);
                }
            }
            if(!tempMetadataValues.empty()){
                boost::mutex::scoped_lock lock(library->resultMutex);
                this->metadataResults[metakey].insert(this->metadataResults[metakey].end(),tempMetadataValues.begin(),tempMetadataValues.end());
            }


        }

		typedef std::vector<std::string> StringVector;

		// Recieve tracks
        if( node.Name()=="tracklist"){
            while( musik::core::xml::ParserNode tracksNode = node.ChildNode("tracks") ){
				tracksNode.WaitForContent();

				StringVector values;
				boost::algorithm::split(values,tracksNode.Content(),boost::algorithm::is_any_of(","));

				try{	// lexical_cast can throw
			        TrackVector tempTrackResults;
			        tempTrackResults.reserve(101);

	                for(StringVector::iterator value=values.begin();value!=values.end();++value){
						int trackId(boost::lexical_cast<DBINT>(*value));
                        tempTrackResults.push_back(TrackPtr(new LibraryTrack(trackId,library->Id())));
					}

					{
		                boost::mutex::scoped_lock lock(library->resultMutex);
		                this->trackResults.insert(this->trackResults.end(),tempTrackResults.begin(),tempTrackResults.end());
					}

				}
				catch(...){
					return false;
				}
			}
		}


		// Recieve trackinfo
        if( node.Name()=="trackinfo"){
			node.WaitForContent();

			StringVector values;
			boost::algorithm::split(values,node.Content(),boost::algorithm::is_any_of(","));

			if(values.size()>=3){
				try{
					boost::mutex::scoped_lock lock(library->resultMutex);
					this->trackInfoTracks	= boost::lexical_cast<UINT64>( values[0] );
					this->trackInfoDuration	= boost::lexical_cast<UINT64>( values[1] );
					this->trackInfoSize		= boost::lexical_cast<UINT64>( values[2] );
				}
				catch(...){
					return false;
				}
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


bool Query::ListBase::RecieveQueryStandardNodes(musik::core::xml::ParserNode &node){
    if(node.Name()=="listeners"){

        // Wait for all content
        node.WaitForContent();

        // Secondly, lets look for what to query for
        // Split comaseparated list
        typedef std::vector<std::string> StringVector;
        StringVector keys;
        boost::algorithm::split(keys,node.Content(),boost::algorithm::is_any_of(","));

        for(StringVector::iterator key=keys.begin();key!=keys.end();++key){
            if(!key->empty()){
                // connect dummy to the signals
                this->OnMetadataEvent(key->c_str()).connect( (Query::ListBase*)this,&Query::ListBase::DummySlot);
            }
        }
    }else if(node.Name()=="listtracks"){
        this->OnTrackEvent().connect( (Query::ListBase*)this,&Query::ListBase::DummySlotTracks);
    }else if(node.Name()=="listtrackinfo"){
        this->OnTrackInfoEvent().connect( (Query::ListBase*)this,&Query::ListBase::DummySlotTrackInfo);
    }

    return true;
}

///   <listeners>genre,artist,album</listeners>
bool Query::ListBase::SendQueryStandardNodes(musik::core::xml::WriterNode &queryNode){
    // Then the listeners
    xml::WriterNode listenersNode(queryNode,"listeners");
    for(MetadataSignals::iterator listener=this->metadataEvent.begin();listener!=this->metadataEvent.end();++listener){
        if( listener->second.has_connections() ){
            if(!listenersNode.Content().empty()){
                listenersNode.Content().append(",");
            }
            listenersNode.Content().append(listener->first);
        }
    }
	// Then the track listener
	if( this->trackEvent.has_connections() ){
	    xml::WriterNode listTracksNode(queryNode,"listtracks");
		listTracksNode.Content().append("true");
	}
	// Then the track listener
	if( this->trackInfoEvent.has_connections() ){
	    xml::WriterNode listTrackInfoNode(queryNode,"listtrackinfo");
		listTrackInfoNode.Content().append("true");
	}
    return true;
}
