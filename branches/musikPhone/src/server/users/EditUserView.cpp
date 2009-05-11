//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright  2007, mC2 Team
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

#include <server/users/EditUserView.hpp>

#include <win32cpp/Frame.hpp>
#include <win32cpp/Label.hpp>
#include <win32cpp/Button.hpp>
#include <win32cpp/LinearLayout.hpp>
#include <win32cpp/EditView.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::server::users;
using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

EditUserView::EditUserView()
: Frame(NULL,WindowPadding(6))
{
}

void EditUserView::OnCreated()
{

    Frame *frame    = this->AddChild(new Frame(0,WindowPadding(4)));
    frame->SetLayoutFlags(LayoutFillFill);

    FontRef boldFont(Font::Create());
    boldFont->SetBold(true);

    // Top Row layout
    LinearLayout* topRowLayout = new LinearLayout(VerticalLayout,LayoutFillFill);
//    topRowLayout->SetDefaultChildFill(false);
//    topRowLayout->SetDefaultChildAlignment(win32cpp::ChildAlignMiddle);

    // First rows column layout
//    LinearLayout* firstColumnLayout = new LinearLayout(HorizontalLayout);
//    firstColumnLayout->SetDefaultChildFill(false);
//    firstColumnLayout->SetDefaultChildAlignment(win32cpp::ChildAlignCenter);

    Label* title    = topRowLayout->AddChild(new Label(_T("Add user")));
    title->SetFont(boldFont);

    // Username
    Label* label   = topRowLayout->AddChild(new Label(_T("Username:") ));
    this->username  = topRowLayout->AddChild(new EditView(160,20 ));

    // Password
    label   = topRowLayout->AddChild(new Label(_T("Password:") ));
    this->password  = topRowLayout->AddChild(new EditView(160,20 ));

    // Nickname
    label   = topRowLayout->AddChild(new Label(_T("Nickname:") ));
    this->nickname = topRowLayout->AddChild(new EditView(160,20 ));

    // Last rows column layout
    LinearLayout* bottomButtonLayout = new LinearLayout(HorizontalLayout);
//    bottomButtonLayout->SetDefaultChildFill(false);
    this->cancelButton  = bottomButtonLayout->AddChild(new Button(_T("Cancel")));
    this->okButton      = bottomButtonLayout->AddChild(new Button(_T("OK")));
    this->cancelButton->Resize(60,20);
    this->okButton->Resize(60,20);
    topRowLayout->AddChild(bottomButtonLayout);
 //   topRowLayout->SetChildAlignment(bottomButtonLayout,ChildAlignRight);


    frame->AddChild(topRowLayout);

}
