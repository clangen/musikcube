//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
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
#include <cube/TransportView.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    TransportView::TransportView()
{
}

void        TransportView::OnCreated()
{
    // main box ----------
    this->mainBox = new BoxLayout(BoxVertical);

    BoxLayout* topRowBox = new BoxLayout(BoxHorizontal);
    Frame* topRowFrame = this->mainBox->AddChild(
        new Frame(topRowBox, FramePadding(4, 0, 2, 0)));

    BoxLayout* bottomRowBox = this->mainBox->AddChild(new BoxLayout(BoxHorizontal));

        // top row box ----------
        this->prevButton = topRowBox->AddChild(new Button(_T("prev")));
        this->playButton = topRowBox->AddChild(new Button(_T("play")));
        this->stopButton = topRowBox->AddChild(new Button(_T("stop")));
        this->nextButton = topRowBox->AddChild(new Button(_T("next")));
        //
        this->prevButton->Resize(50, 28);
        this->playButton->Resize(50, 28);
        this->stopButton->Resize(50, 28);
        this->nextButton->Resize(50, 28);

        // now playingbox
        FontRef boldFont(new win32cpp::Font());
        boldFont->SetBold(true);
        //
        BoxLayout* nowPlayingBox = new BoxLayout(BoxHorizontal);
        Frame* nowPlayingFrame = topRowBox->AddChild(
            new Frame(nowPlayingBox, FramePadding(6, 0, 0, 0)));
        //
        nowPlayingBox->AddChild(new Label(_T("Now playing ")));
        this->titleLabel = nowPlayingBox->AddChild(new Label(_T("Song Title")));
        nowPlayingBox->AddChild(new Label(_T("  by  ")));
        this->artistLabel = nowPlayingBox->AddChild(new Label(_T("Artist Name")));
        //
        this->titleLabel->SetFont(boldFont);
        this->artistLabel->SetFont(boldFont);
        nowPlayingBox->SetSpacing(0);
        nowPlayingBox->ResizeToMinimumSize();
        //
        topRowBox->SetChildFill(nowPlayingFrame);
        topRowBox->SetFlexibleChild(nowPlayingFrame);

        this->volumeSlider = topRowBox->AddChild(new Trackbar());
        this->volumeSlider->Resize(100, 28);

        // bottom row ----------
        BoxLayout* playbackBox = new BoxLayout();
        Frame* playbackFrame = bottomRowBox->AddChild(
            new Frame(playbackBox, FramePadding(6, 6, 0, 0)));
        //
        this->timeElapsedLabel = playbackBox->AddChild(new Label(_T("0:00")));
        this->playbackSlider = playbackBox->AddChild(new Trackbar(0, 100));
        this->timeDurationLabel = playbackBox->AddChild(new Label(_T("0:00")));
        //
        this->playbackSlider->Resize(100, 20);
        playbackBox->SetFlexibleChild(this->playbackSlider);
        playbackBox->ResizeToMinimumSize();

        // size top and bottom rows to minimum size ---------
        topRowBox->ResizeToMinimumSize();
        bottomRowBox->ResizeToMinimumSize();
        bottomRowBox->SetFlexibleChild(playbackFrame);
        bottomRowBox->SetChildFill(playbackFrame);

    // fill top and bottom rows ----------
    this->mainBox->ResizeToMinimumSize();
    this->mainBox->SetDefaultChildFill();
    //
    this->AddChild(this->mainBox);
}
