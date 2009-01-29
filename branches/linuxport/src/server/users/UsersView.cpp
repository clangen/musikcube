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
#include <server/users/UsersView.hpp>

#include <win32cpp/LinearLayout.hpp>
#include <win32cpp/Button.hpp>
#include <win32cpp/ListView.hpp>
#include <win32cpp/Label.hpp>


//////////////////////////////////////////////////////////////////////////////

using namespace musik::server::users;

//////////////////////////////////////////////////////////////////////////////

UsersView::UsersView()
{
}

void UsersView::OnCreated()
{

    LinearLayout* pathLayout = new LinearLayout(HorizontalLayout,win32cpp::LayoutFillFill);
    LinearLayout* pathButtonsLayout = new LinearLayout(VerticalLayout);

    // Path ListView
    this->usersList          = pathLayout->AddChild(new ListView());
    this->usersList->SetLayoutFlags(win32cpp::LayoutFillFill);

    // pathButtons layout
    this->addUserButton     = pathButtonsLayout->AddChild(new Button(_T("Add user")));
    this->removeUserButton  = pathButtonsLayout->AddChild(new Button(_T("Remove user")));

    this->addUserButton->Resize(90, 24);
    this->removeUserButton->Resize(90, 24);

    pathLayout->AddChild(pathButtonsLayout);


    // Add to the layout
    win32cpp::Frame *paddingFrame    = this->AddChild(new Frame(pathLayout,WindowPadding(20)));
    paddingFrame->SetLayoutFlags(win32cpp::LayoutFillFill);

}
