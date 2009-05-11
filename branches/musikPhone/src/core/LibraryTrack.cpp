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

#include <core/LibraryTrack.h>
#include <core/LibraryFactory.h>

#include <core/Common.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>
#include <core/Library/Base.h>

#include <boost/lexical_cast.hpp>
#include <boost/thread/mutex.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::core;

//////////////////////////////////////////////////////////////////////////////

LibraryTrack::LibraryTrack(void)
 :meta(NULL)
 ,id(0)
 ,libraryId(0)
{
}

LibraryTrack::LibraryTrack(DBINT id,int libraryId)
 :meta(NULL)
 ,id(id)
 ,libraryId(libraryId)
{
}

LibraryTrack::LibraryTrack(DBINT id,musik::core::LibraryPtr library)
 :meta(NULL)
 ,id(id)
 ,libraryId(library->Id())
{
}



LibraryTrack::~LibraryTrack(void){
    if(this->meta){
        delete this->meta;
        this->meta  = NULL;
    }

}

const utfchar* LibraryTrack::GetValue(const char* metakey){
    if(metakey && this->meta){
        if(this->meta->library){
            // Threadsafe
            boost::mutex::scoped_lock lock(this->meta->library->trackMutex);
            MetadataMap::iterator metavalue = this->meta->metadata.find(metakey);
            if(metavalue!=this->meta->metadata.end()){
                return metavalue->second.c_str();
            }
        }else{
            MetadataMap::iterator metavalue = this->meta->metadata.find(metakey);
            if(metavalue!=this->meta->metadata.end()){
                return metavalue->second.c_str();
            }
        }
    }
    return NULL;
}

void LibraryTrack::SetValue(const char* metakey,const utfchar* value){
    this->InitMeta();

    if(metakey && value){
        if(this->meta->library){
            // Threadsafe
            boost::mutex::scoped_lock lock(this->meta->library->trackMutex);
            this->meta->metadata.insert(std::pair<std::string,utfstring>(metakey,value));
        }else{
            this->meta->metadata.insert(std::pair<std::string,utfstring>(metakey,value));
        }
    }
}

void LibraryTrack::ClearValue(const char* metakey){
    if(this->meta){
        if(this->meta->library){
            boost::mutex::scoped_lock lock(this->meta->library->trackMutex);
            this->meta->metadata.erase(metakey);
        }else{
            this->meta->metadata.erase(metakey);
        }
    }
}



void LibraryTrack::SetThumbnail(const char *data,long size){
    this->InitMeta();

    if(this->meta->thumbnailData)
        delete this->meta->thumbnailData;

    this->meta->thumbnailData        = new char[size];
    this->meta->thumbnailSize        = size;

    memcpy(this->meta->thumbnailData,data,size);
}

const utfchar* LibraryTrack::URI(){
    static utfstring uri;
    if(this->meta){
        uri     = UTF("mcdb://")+this->meta->library->Identifier()+UTF("/")+boost::lexical_cast<utfstring>(this->id);
        return uri.c_str();
    }else{
        uri     = UTF("mcdb://")+boost::lexical_cast<utfstring>(this->libraryId)+UTF("/")+boost::lexical_cast<utfstring>(this->id);
        return uri.c_str();
    }
    return NULL;
}

const utfchar* LibraryTrack::URL(){
    return this->GetValue("path");
}

Track::MetadataIteratorRange LibraryTrack::GetValues(const char* metakey){
    if(this->meta){
        if(this->meta->library){
            boost::mutex::scoped_lock lock(this->meta->library->trackMutex);
            return this->meta->metadata.equal_range(metakey);
        }else{
            return this->meta->metadata.equal_range(metakey);
        }
    }
    return Track::MetadataIteratorRange();
}

Track::MetadataIteratorRange LibraryTrack::GetAllValues(){
    if(this->meta){
        return Track::MetadataIteratorRange(this->meta->metadata.begin(),this->meta->metadata.end());
    }
    return Track::MetadataIteratorRange();
}

DBINT LibraryTrack::Id(){
    return this->id;
}

musik::core::LibraryPtr LibraryTrack::Library(){
    if(this->meta){
        return this->meta->library;
    }
    return LibraryFactory::Instance().GetLibrary(this->libraryId);
}

int LibraryTrack::LibraryId(){
    return this->libraryId;
}



void LibraryTrack::InitMeta(){
    if(!this->meta){
        // Create the metadata
        this->meta  = new MetaData();
        if(this->libraryId){
            this->meta->library = LibraryFactory::Instance().GetLibrary(this->libraryId);
        }
    }
}



TrackPtr LibraryTrack::Copy(){
    return TrackPtr(new LibraryTrack(this->id,this->libraryId));
}


bool LibraryTrack::GetFileData(DBINT id,db::Connection &db){
    this->InitMeta();

    this->id    = id;

    db::CachedStatement stmt("SELECT t.filename,t.filesize,t.filetime,p.path||f.relative_path||'/'||t.filename FROM tracks t,folders f,paths p WHERE t.folder_id=f.id AND f.path_id=p.id AND t.id=?",db);
    stmt.BindInt(0,id);

    if(stmt.Step()==db::Row){
        this->SetValue("filename"   ,stmt.ColumnTextUTF(0));
        this->SetValue("filesize"   ,stmt.ColumnTextUTF(1));
        this->SetValue("filetime"   ,stmt.ColumnTextUTF(2));
        this->SetValue("path"       ,stmt.ColumnTextUTF(3));
        return true;
    }
    return false;
}

LibraryTrack::MetaData::MetaData()
 :thumbnailData(NULL)
 ,thumbnailSize(0)
{
}


LibraryTrack::MetaData::~MetaData(){
    if(this->thumbnailData)
        delete this->thumbnailData;
}

