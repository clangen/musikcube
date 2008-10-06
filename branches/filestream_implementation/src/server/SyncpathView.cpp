//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, mC2 Team
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
#include <server/SyncpathView.hpp>

#include <win32cpp/LinearLayout.hpp>
#include <win32cpp/Button.hpp>
#include <win32cpp/ListView.hpp>
#include <win32cpp/Label.hpp>


//////////////////////////////////////////////////////////////////////////////

using namespace musik::server;

//////////////////////////////////////////////////////////////////////////////

SyncpathView::SyncpathView()
{
}

void SyncpathView::OnCreated()
{

    LinearLayout* pathLayout = new LinearLayout(LinearColumnLayout);
    LinearLayout* pathButtonsLayout = new LinearLayout(LinearRowLayout);
    

    // Path ListView
    this->pathList          = pathLayout->AddChild(new ListView());

    pathLayout->SetDefaultChildFill(true);
//    pathLayout->SetSizeConstraints(LayoutFillParent,120);
    pathLayout->SetFlexibleChild(this->pathList);


    // pathButtons layout
    this->addPathButton     = pathButtonsLayout->AddChild(new Button(_T("Add path")));
    this->removePathButton  = pathButtonsLayout->AddChild(new Button(_T("Remove path")));

    this->addPathButton->Resize(90, 24);
    this->removePathButton->Resize(90, 24);

    pathButtonsLayout->SetDefaultChildFill(false);
    pathButtonsLayout->SetDefaultChildAlignment(ChildAlignMiddle);
    pathButtonsLayout->SetSizeConstraints(90,LayoutFillParent);

    pathLayout->AddChild(pathButtonsLayout);


    // Add to the layout
    this->AddChild(new Frame(pathLayout,FramePadding(20)));

}
