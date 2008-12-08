//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright  2007, Casey Langen
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
#include <cube/TransportView.hpp>
#include <win32cpp/LinearLayout.hpp>
#include <win32cpp/Label.hpp>
#include <win32cpp/Button.hpp>
#include <win32cpp/Trackbar.hpp>


//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    TransportView::TransportView()
{
}

void        TransportView::OnCreated()
{
    // main layout
    LinearLayout* mainLayout = new LinearLayout(VerticalLayout);
    LinearLayout* topRowLayout = new LinearLayout(HorizontalLayout);
    LinearLayout* bottomRowLayout = new LinearLayout(HorizontalLayout);

    // top row layout
//    topRowLayout->SetDefaultChildFill(false);
//    topRowLayout->SetDefaultChildAlignment(ChildAlignMiddle);

    this->prevButton = topRowLayout->AddChild(new Button(_(_T("Prev"))));
    this->playButton = topRowLayout->AddChild(new Button(_(_T("Play"))));
    this->stopButton = topRowLayout->AddChild(new Button(_(_T("Stop"))));
    this->nextButton = topRowLayout->AddChild(new Button(_(_T("Next"))));
    //
    this->prevButton->Resize(50, 28);
    this->playButton->Resize(50, 28);
    this->stopButton->Resize(50, 28);
    this->nextButton->Resize(50, 28);

    // now playing layout
    FontRef boldFont(Font::Create());
    boldFont->SetBold(true);
    //
    LinearLayout* nowPlayingLayout = new LinearLayout(HorizontalLayout);
    nowPlayingLayout->SetLayoutFlags(win32cpp::LayoutFillFill);
    nowPlayingLayout->SetLayoutAlignment(win32cpp::LayoutAlignMiddle);
    //
    uistring nowPlayingCaption = _(_T("Now playing"));
    nowPlayingCaption += _T(" ");
    nowPlayingLayout->AddChild(new Label(nowPlayingCaption.c_str()));
    this->titleLabel = nowPlayingLayout->AddChild(new Label(_(_T("Song Title"))));
    uistring byCaption = _(_T("by"));
    byCaption = _T(" ") + byCaption + _T(" ");
    nowPlayingLayout->AddChild(new Label(byCaption.c_str()));
    this->artistLabel = nowPlayingLayout->AddChild(new Label(_(_T("Artist Name"))));
    //
    this->titleLabel->SetFont(boldFont);
    this->artistLabel->SetFont(boldFont);
    nowPlayingLayout->SetSpacing(0);
    //
    Frame* nowPlayingFrame = topRowLayout->AddChild(
        new Frame(nowPlayingLayout, FramePadding(6, 0, 0, 0)));
    nowPlayingFrame->SetLayoutFlags(win32cpp::LayoutFillFill);


    this->volumeSlider = topRowLayout->AddChild(new Trackbar());
    this->volumeSlider->Resize(100, 28);

    // bottom row layout
    this->timeElapsedLabel = bottomRowLayout->AddChild(new Label(_T("0:00")));
    this->timeElapsedLabel->SetLayoutAlignment(win32cpp::LayoutAlignMiddle);
    this->playbackSlider = bottomRowLayout->AddChild(new Trackbar(0, 10000));
    this->playbackSlider->SetLayoutFlags(win32cpp::LayoutFillFill);
    this->timeDurationLabel = bottomRowLayout->AddChild(new Label(_T("0:00")));
    this->timeDurationLabel->SetLayoutAlignment(win32cpp::LayoutAlignMiddle);


    // put it all together!
    win32cpp::Frame *topRowFrame    = mainLayout->AddChild(new Frame(topRowLayout, FramePadding(4, 0, 2, 0)));
    topRowFrame->SetLayoutFlags(win32cpp::LayoutFillFill);
    win32cpp::Frame *bottomRowFrame = mainLayout->AddChild(new Frame(bottomRowLayout, FramePadding(6, 6, 0, 0)));
    bottomRowFrame->SetLayoutFlags(win32cpp::LayoutFillFill);
    this->AddChild(mainLayout);
}
