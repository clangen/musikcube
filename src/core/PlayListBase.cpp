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

#include <core/PlayListBase.h>

#include <boost/bind.hpp>

using namespace musik::core;


PlayList::Base::Base(void) : library(NULL),maxCache(100),currentPosition(0){
}

PlayList::Base::~Base(void){
}
/*
void PlayList::Base::init(){
    this->oQuerySongs.addCallback(boost::bind(&PlayList::Base::_updateSongCache,this,_1));
}

void PlayList::Base::_updateSongCache(std::vector<SongPtr> *aSongs){
    // This function will be called in the applications thread (not in the queryparser thread)
    // TODO: Check if the visible range match any of the incomming songs to update.
    if(this->onVisibleSongUpdate){

        if(this->onRequestVisibleRange){
            // Request an update for visible range
            this->onRequestVisibleRange();
        }

        for(std::vector<SongPtr>::iterator oSong=aSongs->begin();oSong!=aSongs->end();++oSong){
            unsigned int iSongPos    = (unsigned int)(*oSong)->iTemp;
            if( (this->iVisibleRangeStart<=iSongPos) && (iSongPos<=this->iVisibleRangeEnd) ){
                // In range
                this->onVisibleSongUpdate(iSongPos);
            }
        }
    }
}

void PlayList::Base::addSongs(SongIdList *aAddSongIds,bool bClear){
    if(bClear){
        // Remove old songs
        this->aSongIdList.clear();
        this->aSongCache.clear();
    }

    if(aAddSongIds!=NULL){
        // Append the songs
        this->aSongIdList.insert(this->aSongIdList.end(),aAddSongIds->begin(),aAddSongIds->end());
    }
}

void PlayList::Base::operator=(const PlayList::Base &oFrom){
    this->aSongIdList        = oFrom.aSongIdList;
    this->aSongCache        = oFrom.aSongCache;
    this->iCurrentPosition    = oFrom.iCurrentPosition;
}

SongPtr PlayList::Base::getSongFromPosition(unsigned int iPos,bool bWait){

    if(iPos<this->aSongIdList.size()){
        DBINT iSongId    = this->aSongIdList[iPos];
        SongCache::iterator oSong    = this->aSongCache.find(iSongId);
        if(oSong!=this->aSongCache.end()){
            if(oSong->second->GetStatus()&State::Finished){
                return oSong->second;
            }
        }
    }

    return SongPtr();
}

SongPtr PlayList::Base::getSongFromId(DBINT iId,bool bWait){


    return SongPtr();
}

void PlayList::Base::cacheSongs(unsigned int iStartRange,unsigned int iEndRange){
//    SongIdList aQuerySongs;
    DBINT iNOFFSongs    = this->aSongIdList.size();
    bool bSendQuery(false);

    if(iStartRange==iEndRange){
        iEndRange += this->iVisibleRangeEnd-this->iVisibleRangeStart;
    }

    for(unsigned int i(iStartRange);i<=iEndRange && i<iNOFFSongs;++i){
        DBINT iSongId    = this->aSongIdList[i];
//        SongCache::iterator oSong=this->aSongCache.find(iSongId);
//        if( oSong == this->aSongCache.end() ){
        if( this->aSongCache.find(iSongId)==this->aSongCache.end() ){
            // Song is not in cache, query for it.
            this->aSongCache[iSongId]            = this->oQuerySongs.requestSong(iSongId);
            this->aSongCache[iSongId]->iTemp    = i;
            bSendQuery    = true;
        }
    }
    if(bSendQuery){
        // Query for the songs
//FIXME        this->oLibraryPtr->AddQuery(new Query::Songs(this->oQuerySongs),DOEQUERY_CLEARNONE,true);
        this->oQuerySongs.clearSongs();
    }
}

unsigned int PlayList::Base::getCount(){
    return this->aSongIdList.size();
}

void PlayList::Base::setVisibleRange(unsigned int iStartRange,unsigned int iEndRange){
    this->iVisibleRangeStart    = iStartRange;
    this->iVisibleRangeEnd        = iEndRange;
}

void PlayList::Base::setLibrary(Library::Base *oNewLibraryPtr){
    this->oLibraryPtr    = oNewLibraryPtr;
}




*/
