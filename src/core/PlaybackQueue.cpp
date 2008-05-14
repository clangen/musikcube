//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, mC2 team
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
#include "core/PlaybackQueue.h"
#include "core/LibraryFactory.h"

#include <boost/algorithm/string.hpp>


using namespace musik::core;

//////////////////////////////////////////////////////////////////////////////

PlaybackQueue PlaybackQueue::sInstance;


//////////////////////////////////////////
///\brief
///Constructor
///
///Will connect the appropiate signals in the transport
//////////////////////////////////////////
PlaybackQueue::PlaybackQueue(void) :
    nowPlaying( new musik::core::tracklist::Standard() ),
    signalDisabled(false),
    playing(false)
{
    this->transport.MixpointReached.connect(this,&PlaybackQueue::OnPlaybackEndOrFail);
//    this->transport.PlaybackStoppedFail.connect(this,&PlaybackQueue::OnPlaybackEndOrFail);
}

//////////////////////////////////////////
///\brief
///Destructor
//////////////////////////////////////////
PlaybackQueue::~PlaybackQueue(void)
{
    this->transport.Stop(0);
}

//////////////////////////////////////////
///\brief
///Private method for the transporters signals
//////////////////////////////////////////
void PlaybackQueue::OnPlaybackEndOrFail(){
    this->playing   = false;
    if(!this->signalDisabled){
        this->Next();
    }
}

//////////////////////////////////////////
///\brief
///Return a shared_ptr to the now playing tracklist
//////////////////////////////////////////
tracklist::Standard::Ptr PlaybackQueue::NowPlayingTracklist(){
    return this->nowPlaying;
}

//////////////////////////////////////////
///\brief
///Start playing the current track.
//////////////////////////////////////////
void PlaybackQueue::Play(){

    TrackPtr track(this->CurrentTrack());

    if(track){
        // If current track exists
        utfstring path(track->GetValue("path"));

        this->Stop();

        this->playing   = true;
        this->transport.Start(path);
    }
}

//////////////////////////////////////////
///\brief
///Start playing the next track.
//////////////////////////////////////////
void PlaybackQueue::Next(){
    musik::core::TrackPtr track( this->nowPlaying->NextTrack() );

    this->SetCurrentTrack(track);
    this->Play();
}

//////////////////////////////////////////
///\brief
///Start playing the previous track.
//////////////////////////////////////////
void PlaybackQueue::Previous(){
    musik::core::TrackPtr track( this->nowPlaying->PreviousTrack() );

	this->SetCurrentTrack(track);
	this->Play();

}

//////////////////////////////////////////
///\brief
///Stop playing
//////////////////////////////////////////
void PlaybackQueue::Stop(){
    this->signalDisabled    = true;
    if( this->playing ){
        this->transport.Stop(0);
    }
    this->signalDisabled    = false;
}


//////////////////////////////////////////
///\brief
///Return the current running track
//////////////////////////////////////////
TrackPtr PlaybackQueue::CurrentTrack(){
    if (this->nowPlaying->Size() <= 0)
    {
        return TrackPtr();
    }

    if(!this->currentTrack){
        // If the current track is empty, get a track from the nowPlaying tracklist
        this->SetCurrentTrack( this->nowPlaying->CurrentTrack() );
    }
    return this->currentTrack;
}

//////////////////////////////////////////
///\brief
///Set the current track
///
///Setting the current track will also query 
///for all the tracks metadata
//////////////////////////////////////////
void PlaybackQueue::SetCurrentTrack(TrackPtr track){

	if(track){
		this->currentTrack  = track->Copy();
	}else{
		this->currentTrack	= musik::core::TrackPtr();
	}

    // Get all metadata to the track
    if(this->currentTrack){
        this->metadataQuery.Clear();
        this->metadataQuery.RequestAllMetakeys();
        this->metadataQuery.RequestTrack(this->currentTrack);
        this->nowPlaying->Library()->AddQuery(this->metadataQuery,musik::core::Query::Wait|musik::core::Query::AutoCallback|musik::core::Query::UnCanceable|musik::core::Query::Prioritize);
    }

    // Call the signal if track updates
    this->CurrentTrackChanged(this->currentTrack);

}

//////////////////////////////////////////
///\brief
///Copy the tracklist to the now playing and start playing it
///
///\param tracklist
///Tracklist that should be copied to now playing
//////////////////////////////////////////
void PlaybackQueue::Play(tracklist::IRandomAccess &tracklist){
    this->currentTrack.reset();
    this->nowPlaying->CopyTracks(tracklist);
    this->Play();
}