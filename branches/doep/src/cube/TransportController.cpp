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

#include "pch.hpp"

#include <boost/format.hpp>

#include <cube/TransportController.hpp>
#include <win32cpp/ApplicationThread.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    TransportController::TransportController(TransportView& transportView)
: transportView(transportView)
, playbackSliderTimer(500)
, playbackSliderMouseDown(false)
, paused(false)
, playing(false)
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
        musik::core::PlaybackQueue::Instance().Transport().Volume());

    musik::core::PlaybackQueue::Instance().CurrentTrackChanged.connect(this,&TransportController::OnTrackChange);

    this->transportView.playbackSlider->Repositioned.connect(
        this, &TransportController::OnPlaybackSliderChange);

    this->transportView.playbackSlider->MouseButtonDown.connect(
        this, &TransportController::OnPlaybackSliderMouseDown);

    this->transportView.playbackSlider->MouseButtonUp.connect(
        this, &TransportController::OnPlaybackSliderMouseUp);

    musik::core::PlaybackQueue::Instance().Transport().EventPlaybackStartedOk.connect(this, &TransportController::OnPlaybackStarted);
    musik::core::PlaybackQueue::Instance().Transport().EventPlaybackStoppedOk.connect(this, &TransportController::OnPlaybackStopped);

    musik::core::PlaybackQueue::Instance().Transport().EventPlaybackPausedOk.connect(this, &TransportController::OnPlaybackPaused);
    musik::core::PlaybackQueue::Instance().Transport().EventPlaybackResumedOk.connect(this, &TransportController::OnPlaybackResumed);

    this->playbackSliderTimer.ConnectToWindow(this->transportView.playbackSlider);

    this->playbackSliderTimer.OnTimeout.connect(this, &TransportController::OnPlaybackSliderTimerTimedOut);
}

void        TransportController::OnViewResized(Window* window, Size size)
{
}

void        TransportController::OnPlayPressed(Button* button)
{
    if (!this->playing)     musik::core::PlaybackQueue::Instance().Play();
    else if (this->paused)  musik::core::PlaybackQueue::Instance().Resume();
    else                    musik::core::PlaybackQueue::Instance().Pause();
}

void        TransportController::OnStopPressed(Button* button)
{
    musik::core::PlaybackQueue::Instance().Stop();
    this->transportView.playbackSlider->SetPosition(0);
    this->playbackSliderTimer.Stop();
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
    musik::core::PlaybackQueue::Instance().Transport().SetVolume(transportView.volumeSlider->Position());
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
    unsigned long lengthMs = musik::core::PlaybackQueue::Instance().Transport().TrackLength();
    unsigned long newPosMs = lengthMs * trackBar->Position() / trackBar->Range();

    musik::core::PlaybackQueue::Instance().Transport().SetTrackPosition(newPosMs);
}

void TransportController::OnPlaybackSliderTimerTimedOut()
{
    unsigned long currPosMs = musik::core::PlaybackQueue::Instance().Transport().TrackPosition();
    unsigned long lengthMs = musik::core::PlaybackQueue::Instance().Transport().TrackLength();
    unsigned long sliderRange = this->transportView.playbackSlider->Range();

    this->transportView.timeElapsedLabel->SetCaption(this->FormatTime(currPosMs));

    if (!this->playbackSliderMouseDown && lengthMs != 0)
    {
        this->transportView.playbackSlider->SetPosition(sliderRange * currPosMs / lengthMs);
    }
}

void TransportController::OnPlaybackSliderMouseDown(Window* windows, MouseEventFlags flags, Point point)
{
    this->playbackSliderMouseDown = true;
}

void TransportController::OnPlaybackSliderMouseUp(Window* windows, MouseEventFlags flags, Point point)
{
    this->playbackSliderMouseDown = false;
}

void TransportController::OnPlaybackStarted(musik::core::TrackPtr track)
{
    if(!win32cpp::ApplicationThread::InMainThread())
    {
        win32cpp::ApplicationThread::Call1(this, &TransportController::OnPlaybackStarted, track);
        return;
    }

    this->playing = true;
    this->transportView.playButton->SetCaption(_T("Pause"));

    this->transportView.timeDurationLabel->SetCaption(this->FormatTime(musik::core::PlaybackQueue::Instance().Transport().TrackLength()));
    
    this->transportView.playbackSlider->SetPosition(0);
    
    this->playbackSliderTimer.Start();

    this->displayedTrack = track;
}

void TransportController::OnPlaybackStopped(musik::core::TrackPtr track)
{
    if(!win32cpp::ApplicationThread::InMainThread())
    {
        win32cpp::ApplicationThread::Call1(this, &TransportController::OnPlaybackStopped, track);
        return;
    }

    if (this->displayedTrack->id == track->id) // For out of order signals
    {
        this->playing = false;
        this->paused = false;

        this->transportView.playButton->SetCaption(_T("Play"));

        this->transportView.playbackSlider->SetPosition(0);
        this->playbackSliderTimer.Stop();  
        
        this->transportView.timeElapsedLabel->SetCaption(_T("0:00"));
        this->transportView.timeDurationLabel->SetCaption(_T("0:00"));
    }
}

void TransportController::OnPlaybackPaused()
{
    if(!win32cpp::ApplicationThread::InMainThread())
    {
        win32cpp::ApplicationThread::Call0(this, &TransportController::OnPlaybackPaused);
        return;
    }

    this->paused = true;
    this->transportView.playButton->SetCaption(_T("Resume"));
}

void TransportController::OnPlaybackResumed()
{
    if(!win32cpp::ApplicationThread::InMainThread())
    {
        win32cpp::ApplicationThread::Call0(this, &TransportController::OnPlaybackResumed);
        return;
    }

    this->paused = false;
    this->transportView.playButton->SetCaption(_T("Pause"));
}

win32cpp::uistring  TransportController::FormatTime(unsigned long ms)
{
    unsigned long seconds = ms / 1000 % 60;
    unsigned long minutes = ms / 1000 / 60;
    
    boost::basic_format<uichar> format(_T("%1%:%2$02d"));
    format % minutes;
    format % seconds;
    
    return format.str();
}