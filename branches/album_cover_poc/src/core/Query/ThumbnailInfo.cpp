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
#include <core/Query/ThumbnailInfo.h>
#include <core/Library/Base.h>

using namespace musik::core;

Query::ThumbnailInfo::ThumbnailInfo(void)
: trackId(0)
{
}

Query::ThumbnailInfo::~ThumbnailInfo(void){
}

void Query::ThumbnailInfo::GetThumbnail(int trackId){
    this->trackId   = trackId;
}

bool Query::ThumbnailInfo::ParseQuery(Library::Base *oLibrary,db::Connection &db){
    db::Statement stmt("SELECT tn.id||\".jpg\" FROM tracks t,thumbnails tn WHERE t.id=? AND t.thumbnail_id=tn.id",db);
    stmt.BindInt(0,this->trackId);
    if(stmt.Step()==db::Row){
        this->filename  = oLibrary->GetLibraryDirectory()+UTF("thumbs/");
        this->filename  += stmt.ColumnTextUTF(0);
    }

    return true;
}

Query::Ptr Query::ThumbnailInfo::copy() const{
    return Query::Ptr(new Query::ThumbnailInfo(*this));
}

bool Query::ThumbnailInfo::RunCallbacks(Library::Base *oLibrary){
    this->ThumbnailInfoFetched(this->filename);
    return true;
}
