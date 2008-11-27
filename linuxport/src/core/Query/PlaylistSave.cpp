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
#include <core/Query/PlaylistSave.h>
#include <core/Library/Base.h>

using namespace musik::core;


//////////////////////////////////////////
///\brief
///Constructor
//////////////////////////////////////////
Query::PlaylistSave::PlaylistSave(void){
}

//////////////////////////////////////////
///\brief
///Destructor
//////////////////////////////////////////
Query::PlaylistSave::~PlaylistSave(void){
}

void Query::PlaylistSave::SavePlaylist(const utfstring playlistName,int playlistId,musik::core::tracklist::IRandomAccess *tracklist){
    this->playlistId    = playlistId;
    this->playlistName  = playlistName;

	this->tracks.clear();

	if(tracklist){
		for(int i(0);i<tracklist->Size();++i){
			this->tracks.push_back((*tracklist)[i]->id);
		}
	}
}

bool Query::PlaylistSave::ParseQuery(Library::Base *library,db::Connection &db){

    db::ScopedTransaction transaction(db);

    {
        db::Statement updatePlaylist("INSERT OR REPLACE INTO playlists (id,name,user_id) VALUES (?,?,?)",db);
        if(this->playlistId!=0){
            updatePlaylist.BindInt(0,this->playlistId);
        }
        updatePlaylist.BindTextUTF(1,this->playlistName);
		updatePlaylist.BindInt(2,library->userId);

        if( updatePlaylist.Step()==db::Done ){
            if(this->playlistId==0){
                this->playlistId    = db.LastInsertedId();
            }
        }
    }

    {
        db::Statement deleteTracks("DELETE FROM playlist_tracks WHERE playlist_id=?",db);
        deleteTracks.BindInt(0,this->playlistId);
        deleteTracks.Step();
    }

    db::Statement insertTracks("INSERT INTO playlist_tracks (track_id,playlist_id,sort_order) VALUES (?,?,?)",db);

    for(int i(0);i<this->tracks.size();++i){
        insertTracks.BindInt(0,this->tracks[i]);
        insertTracks.BindInt(1,this->playlistId);
        insertTracks.BindInt(2,i);
        insertTracks.Step();
        insertTracks.Reset();
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
Query::Ptr Query::PlaylistSave::copy() const{
    return Query::Ptr(new Query::PlaylistSave(*this));
}

bool Query::PlaylistSave::RunCallbacks(Library::Base *library){
	bool callCallbacks(false);
	{
		boost::mutex::scoped_lock lock(library->libraryMutex);
		if( (this->status & Status::Ended)!=0){
			callCallbacks	= true;
		}
	}

	if(callCallbacks){
        this->PlaylistSaved(this->playlistId);
	}
    return callCallbacks;
}


