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
#include <core/Query/Playlists.h>
#include <core/Library/Base.h>
#include <core/tracklist/Playlist.h>

using namespace musik::core;


//////////////////////////////////////////
///\brief
///Constructor
//////////////////////////////////////////
Query::Playlists::Playlists(void){
}

//////////////////////////////////////////
///\brief
///Destructor
//////////////////////////////////////////
Query::Playlists::~Playlists(void){
}


bool Query::Playlists::ParseQuery(Library::Base *library,db::Connection &db){

    db::Statement stmt("SELECT id,name FROM playlists WHERE user_id=?",db);

	stmt.BindInt(0,library->userId);

    while(stmt.Step()==db::Row){
        tracklist::Ptr playlist( new tracklist::Playlist(
                stmt.ColumnInt(0),
                stmt.ColumnTextUTF(1),
                library->GetSelfPtr()
            ) );

        this->tracklistVector.push_back(playlist); 
    }

    return true;
}

//////////////////////////////////////////
///\brief
///Copy a query
///
///\returns
///A shared_ptr to the Query::Base
//////////////////////////////////////////
Query::Ptr Query::Playlists::copy() const{
    return Query::Ptr(new Query::Playlists(*this));
}

bool Query::Playlists::RunCallbacks(Library::Base *library){
    bool bReturn(false);
    {
        boost::mutex::scoped_lock lock(library->libraryMutex);
        if( (this->status & Status::Ended)!=0){
            // If the query is finished, this function should return true to report that it is finished.
            bReturn    = true;
        }
    }

    if(bReturn){
        this->PlaylistList(this->tracklistVector);
    }

    return bReturn;
}

