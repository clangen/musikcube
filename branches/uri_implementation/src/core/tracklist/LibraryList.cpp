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
#include <core/tracklist/LibraryList.h>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::core::tracklist;

//////////////////////////////////////////////////////////////////////////////

LibraryList::LibraryList(musik::core::LibraryPtr library)
 :library(library)
 ,currentPosition(-1)
{

}


musik::core::TrackPtr LibraryList::operator [](long position){
    return musik::core::TrackPtr();
}

musik::core::TrackPtr LibraryList::TrackWithMetadata(long position){
    return musik::core::TrackPtr();
}

musik::core::TrackPtr LibraryList::CurrentTrack(){
    if(this->SetPosition(this->currentPosition)){
//TODO
    }
    return musik::core::TrackPtr();
}

musik::core::TrackPtr LibraryList::NextTrack(){
    return musik::core::TrackPtr();
}

musik::core::TrackPtr LibraryList::PreviousTrack(){
    return musik::core::TrackPtr();
}


bool LibraryList::SetPosition(long position){
    if(position>=0 && position<this->tracklist.size()){
        this->currentPosition   = position;
        return true;
    }

    this->currentPosition   = -1;
    return false;
}

long LibraryList::CurrentPosition(){
    return this->currentPosition;
}

long LibraryList::Size(){
    return this->tracklist.size();
}

void LibraryList::Clear(){
    this->tracklist.clear();
    this->currentPosition   = -1;
}


bool LibraryList::operator =(musik::core::tracklist::Base &tracklist){
    this->Clear();
    this->tracklist.reserve(tracklist.Size());

    int libraryId   = this->library->Id();
    bool success(false);
    // Loop through the tracks and copy everything
    for(long i(0);i<tracklist.Size();++i){
        musik::core::TrackPtr currentTrack  = tracklist[i];
        if(currentTrack->Id()==libraryId){
            // Same library, append
            this->tracklist.push_back(currentTrack->Id());
            success=true;
        }
    }

    return false;
}

//////////////////////////////////////////
///\brief
///Append tracks from another tracklist
///
///It will append all tracks that comes from
///the same library and ignore the rest.
//////////////////////////////////////////
bool LibraryList::operator +=(musik::core::tracklist::Base &tracklist){

    this->tracklist.reserve(tracklist.Size()+this->Size());

    int libraryId   = this->library->Id();
    bool success(false);
    // Loop through the tracks and copy everything
    for(long i(0);i<tracklist.Size();++i){
        musik::core::TrackPtr currentTrack  = tracklist[i];
        if(currentTrack->Id()==libraryId){
            // Same library, append
            this->tracklist.push_back(currentTrack->Id());
            success=true;
        }
    }

    return success;
}

bool LibraryList::operator +=(musik::core::TrackPtr track){
    if(this->library->Id()==track->LibraryId()){
        this->tracklist.push_back(track->Id());
        return true;
    }
    return false;
}

musik::core::LibraryPtr LibraryList::Library(){
    return this->library;
}


