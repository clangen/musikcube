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

#include <core/TrackMeta.h>
#include <core/ThreadHelper.h>

using namespace musik::core;

TrackMeta::TrackMeta(Library::Base *library) : library(library),thumbnailData(NULL),thumbnailSize(0){
}

TrackMeta::~TrackMeta(void){
    if(this->thumbnailData)
        delete this->thumbnailData;
}

//////////////////////////////////////////
///\brief
///Get value of a (one) tag
///
///\param key
///String identifier of the required value.
///
///\param threadHelper
///If you pass this ThreadHelper object, the GetValue will be threadsafe.
///
///\returns
///A const reference to a wstring (UTF-16) with the value. Empty string if not found.
//////////////////////////////////////////
const TrackMeta::Value& TrackMeta::GetValue(const TrackMeta::Key &key) const{

    if(this->library){
        boost::mutex::scoped_lock lock(this->library->libraryMutex);
        return this->_GetValue(key);
    }

    return this->_GetValue(key);
}

const utfchar* TrackMeta::GetValue(const char* metakey) const{
    if(this->library){
        boost::mutex::scoped_lock lock(this->library->libraryMutex);

        TrackMeta::TagMap::const_iterator oKeyValue = this->tags.find(metakey);
        if(oKeyValue!=this->tags.end()){
            return oKeyValue->second.c_str();
        }
        return NULL;
    }

    TrackMeta::TagMap::const_iterator oKeyValue = this->tags.find(metakey);
    if(oKeyValue!=this->tags.end()){
        return oKeyValue->second.c_str();
    }
    return NULL;
}


//////////////////////////////////////////
///\brief
///Get all values of one tag identifier.
///
///\param key
///String identifier of the required value.
///
///\param threadHelper
///If you pass this ThreadHelper object, the GetValue will be threadsafe.
///
///\returns
///A pair of iterators. The first is the start iterator and the second is the end.
///
///\throws <exception class>
///Description of criteria for throwing this exception.
///
///\see
///GetValue
//////////////////////////////////////////
TrackMeta::TagMapIteratorPair TrackMeta::GetValues(const char* metakey) const{

    if(this->library){
        boost::mutex::scoped_lock lock(library->libraryMutex);
        return this->tags.equal_range(metakey);
    }
    return this->tags.equal_range(metakey);

}

void TrackMeta::SetValue(const TrackMeta::Key &key,const TrackMeta::Value &value){
    if(this->library){
        // Threadsafe
        boost::mutex::scoped_lock lock(library->libraryMutex);
        this->tags.insert( TrackMeta::TagMapPair(key,value) );
        return;
    }

    // Non threadsafe
    this->tags.insert( TrackMeta::TagMapPair(key,value) );
}

const TrackMeta::Value& TrackMeta::_GetValue(const TrackMeta::Key &key) const{

    TrackMeta::TagMap::const_iterator oKeyValue = this->tags.find(key);
    if(oKeyValue!=this->tags.end()){
        return oKeyValue->second;
    }
    static TrackMeta::Value emptyString;
    return emptyString;

}

void TrackMeta::SetThumbnail(const char *data,unsigned int size){
    if(this->thumbnailData)
        delete this->thumbnailData;

    this->thumbnailData        = new char[size];
    this->thumbnailSize        = size;

    memcpy(this->thumbnailData,data,size);
}
