//////////////////////////////////////////////////////////////////////////////
// Copyright  2007, mC2 team
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
#include <core/PlaybackQueue.h>
#include <core/LibraryFactory.h>
#include <core/tracklist/MultiLibraryList.h>

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
PlaybackQueue::PlaybackQueue(void) 
 :signalDisabled(false)
 ,playing(false)
 ,paused(false)
{
    this->nowPlaying.reset(new tracklist::MultiLibraryList());
//    this->transport.EventMixpointReached.connect(this,&PlaybackQueue::OnPlaybackEndOrFail);
    this->transport.PlaybackEnded.connect(this,&PlaybackQueue::OnPlaybackEndOrFail);
    this->transport.PlaybackAlmostDone.connect(this,&PlaybackQueue::OnPlaybackPrepare);
}

//////////////////////////////////////////
///\brief
///Destructor
//////////////////////////////////////////
PlaybackQueue::~PlaybackQueue(void)
{
    this->signalDisabled    = true;
    this->transport.Stop();
}

//////////////////////////////////////////
///\brief
///Private method for the transporters signals
//////////////////////////////////////////
void PlaybackQueue::OnPlaybackEndOrFail(){
    this->playing   = false;
    this->paused    = false;

    if(!this->signalDisabled){
        this->Next();
    }
}

void PlaybackQueue::OnPlaybackPrepare(){
    // Try to get the next track
    long pos    = this->nowPlaying->CurrentPosition();
	musik::core::TrackPtr track( (*this->nowPlaying)[pos+1] );

    if(track){
        this->GetAllTrackMetadata(track);
        this->transport.PrepareNextTrack(track->URL());
    }
}


//////////////////////////////////////////
///\brief
///Return a shared_ptr to the now playing tracklist
//////////////////////////////////////////
tracklist::Ptr PlaybackQueue::NowPlayingTracklist(){
    return this->nowPlaying;
}

//////////////////////////////////////////
///\brief
///Start playing the current track.
//////////////////////////////////////////
void PlaybackQueue::Play(){
//    if( !this->nowPlaying->Library()->Exited() ){
        TrackPtr track(this->CurrentTrack());

        this->Stop();

        if(track){

            this->playing   = true;
            this->transport.Start(track->URL()); 

            this->paused    = false;
        }
//    }
}

//////////////////////////////////////////
///\brief
///Pause the currently playing track.
//////////////////////////////////////////
void PlaybackQueue::Pause()
{
    if(this->playing && !this->paused)
    {
        if (this->transport.Pause())
            this->paused = true;
    }
}

//////////////////////////////////////////
///\brief
///Resume the track.
//////////////////////////////////////////
void PlaybackQueue::Resume()
{
    if(this->playing && this->paused)
    {
        if (this->transport.Resume())
            this->paused = false;
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
        this->transport.Stop();
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
        this->currentTrack    = musik::core::TrackPtr();
    }

    bool isNextTrack(false);

    if(this->currentTrack && this->nextTrack){
        utfstring trackURI( this->currentTrack->URI() );
        utfstring nextTrackURI( this->nextTrack->URI() );
        if(trackURI==nextTrackURI){
            this->currentTrack  = this->nextTrack;
            isNextTrack = true;
        }
    }

    this->nextTrack.reset();

    if(!isNextTrack){
        // Get all metadata to the track
        this->GetAllTrackMetadata(this->currentTrack);
    }

    // Call the signal if track updates
    this->CurrentTrackChanged(this->currentTrack);

}

void PlaybackQueue::GetAllTrackMetadata(TrackPtr track){
    if(track){
        this->metadataQuery.Clear();
        this->metadataQuery.RequestAllMetakeys();
        this->metadataQuery.RequestTrack(track);
        LibraryPtr library  = track->Library();
        if(library){
            library->AddQuery(this->metadataQuery,musik::core::Query::Wait|musik::core::Query::AutoCallback|musik::core::Query::UnCanceable|musik::core::Query::Prioritize);
        }
    }
}


//////////////////////////////////////////
///\brief
///Copy the tracklist to the now playing and start playing it
///
///\param tracklist
///Tracklist that should be copied to now playing
//////////////////////////////////////////
void PlaybackQueue::Play(tracklist::Base &tracklist){

	// Set the "now playing" to libraries own playlist
	(*this->nowPlaying)	= tracklist;

	this->currentTrack.reset();
    this->Play();
}

void PlaybackQueue::Append(tracklist::Base &tracklist){
	// Set the "now playing" to libraries own playlist
	(*this->nowPlaying)	+= tracklist;
}
