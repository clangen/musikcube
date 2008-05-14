//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, musikCube team
//
// Sources and Binaries of: mC2, win32cpp
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

#include <pch.hpp>
#include <cube/TransportController.hpp>
#include <win32cpp/ApplicationThread.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    TransportController::TransportController(TransportView& transportView)
: transportView(transportView)
{
    this->transportView.Created.connect(
        this, &TransportController::OnViewCreated);
    
    this->transportView.Resized.connect(
        this, &TransportController::OnViewResized);
}

void        TransportController::OnViewCreated(Window* window)
{
    this->transportView.playButton->Pressed.connect(
        this, &TransportController::OnPlayPressed);

    this->transportView.stopButton->Pressed.connect(
        this, &TransportController::OnStopPressed);

    this->transportView.nextButton->Pressed.connect(
        this, &TransportController::OnNextPressed);

    this->transportView.prevButton->Pressed.connect(
        this, &TransportController::OnPreviousPressed);

    this->transportView.volumeSlider->Repositioned.connect(
        this, &TransportController::OnVolumeSliderChange);

    this->transportView.volumeSlider->SetPosition(
        musik::core::PlaybackQueue::Instance().Volume());

    musik::core::PlaybackQueue::Instance().CurrentTrackChanged.connect(this,&TransportController::OnTrackChange);

    this->transportView.playbackSlider->Repositioned.connect(
        this, &TransportController::OnPlaybackSliderChange);
}

void        TransportController::OnViewResized(Window* window, Size size)
{
}

void        TransportController::OnPlayPressed(Button* button)
{
    musik::core::PlaybackQueue::Instance().Play();
}

void        TransportController::OnStopPressed(Button* button)
{
    musik::core::PlaybackQueue::Instance().Stop();
}

void        TransportController::OnNextPressed(Button* button)
{
    musik::core::PlaybackQueue::Instance().Next();
}

void        TransportController::OnPreviousPressed(Button* button)
{
    musik::core::PlaybackQueue::Instance().Previous();
}

void        TransportController::OnVolumeSliderChange(Trackbar* trackbar)
{
    musik::core::PlaybackQueue::Instance().SetVolume(transportView.volumeSlider->Position());
}

void TransportController::OnTrackChange(musik::core::TrackPtr track){
    if(!win32cpp::ApplicationThread::InMainThread()){
        win32cpp::ApplicationThread::Call1(this,&TransportController::OnTrackChange,track);
        return;
    }

    win32cpp::uistring title(_T("-"));
    win32cpp::uistring artist(_T("-"));

	if(track){

		if(track->GetValue("title"))
			title.assign( track->GetValue("title") );

		if(track->GetValue("visual_artist"))
			artist.assign( track->GetValue("visual_artist") );

	}

    this->transportView.titleLabel->SetCaption(title);
    this->transportView.artistLabel->SetCaption(artist);

}

void TransportController::OnPlaybackSliderChange(Trackbar *trackBar)
{
    musik::core::PlaybackQueue::Instance().JumpToPosition(trackBar->Position());
}