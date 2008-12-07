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

#include <core/NonLibraryTrackHelper.h>
#include <boost/bind.hpp>
#include <core/PluginFactory.h>
#include <core/Plugin/IMetaDataReader.h>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::core;

//////////////////////////////////////////////////////////////////////////////

NonLibraryTrackHelper NonLibraryTrackHelper::sInstance;


NonLibraryTrackHelper::NonLibraryTrackHelper(void)
 :threadIsRunning(false)
{
}

NonLibraryTrackHelper::~NonLibraryTrackHelper(void){
}

NonLibraryTrackHelper& NonLibraryTrackHelper::Instance(){
    return NonLibraryTrackHelper::sInstance;
}

boost::mutex& NonLibraryTrackHelper::TrackMutex(){
    return NonLibraryTrackHelper::sInstance.trackMutex;
}

void NonLibraryTrackHelper::ReadTrack(musik::core::TrackPtr track){
    bool threadRunning(false);
    {
        boost::mutex::scoped_lock lock(this->mutex);
        this->tracksToRead.push_back(TrackWeakPtr(track));
        threadRunning   = this->threadIsRunning;
    }

    if(!threadRunning){
        if(this->helperThread){
            this->helperThread->join();
        }
        this->helperThread.reset(new boost::thread(boost::bind(&NonLibraryTrackHelper::ThreadLoop,this)));

    }
}

void NonLibraryTrackHelper::ThreadLoop(){

    // Get the metadatareaders
    typedef Plugin::IMetaDataReader PluginType;
    typedef PluginFactory::DestroyDeleter<PluginType> Deleter;
    typedef std::vector<boost::shared_ptr<Plugin::IMetaDataReader>> MetadataReaderList;
    MetadataReaderList metadataReaders = PluginFactory::Instance().QueryInterface<PluginType, Deleter>("GetMetaDataReader");


    // pop a track, read the metadata, notify and continue
    bool moreTracks(true);
    while(moreTracks){
        moreTracks  = false;
        musik::core::TrackPtr track;
        {
            boost::mutex::scoped_lock lock(this->mutex);

            if(!this->tracksToRead.empty()){
                // is this a valid track
                track = this->tracksToRead.front().lock();
                this->tracksToRead.pop_front();
            }

            moreTracks  = !this->tracksToRead.empty();

            if(!moreTracks){
                // Set to "not running". No locking beyond this point. 
                this->threadIsRunning   = false;
            }
        }
        if(track){
            utfstring url(track->URL());
            utfstring::size_type lastDot = url.find_last_of(UTF("."));
            if(lastDot!=utfstring::npos){
                track->SetValue("extension",url.substr(lastDot+1).c_str());
            }
            // Read track metadata
            typedef MetadataReaderList::iterator Iterator;
            Iterator it = metadataReaders.begin();
            while (it != metadataReaders.end()) {
                if((*it)->CanReadTag(track->GetValue("extension")) ){
                    // Should be able to read the tag
                    if( (*it)->ReadTag(track.get()) ){
                        // Successfully read the tag.
//                        tagRead=true;
                    }
                }

                it++;
            }

            // Lets notify that tracks has been read
            this->TrackMetadataUpdated(track);
        }

    }

}


