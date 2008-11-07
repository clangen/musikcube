//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright  2008, mC2 Team
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
#include <core/Common.h>
#include <cube/dialog/PreferencesView.hpp>
#include <cube/dialog/PreferencesCategoriesModel.hpp>
#include <win32cpp/Button.hpp>
#include <win32cpp/LinearLayout.hpp>


//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube::dialog;
using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

PreferencesView::PreferencesView()
: Frame(NULL,FramePadding(5))
{
}

void PreferencesView::OnCreated()
{
    // Main Layout:
    // - Row 1: ColumnLayout with CategoryList and Dialog
    // - Row 2: ColumnLayout with Buttons to cancel and apply
    this->mainLayout = new LinearLayout(LinearRowLayout);
    this->mainLayout->SetDefaultChildFill(true);
    this->mainLayout->SetSizeConstraints(LayoutFillParent, LayoutFillParent);
    {    
        // Row 1
        LinearLayout* settingsLayout = new LinearLayout(LinearColumnLayout);
        {
            this->categoryList = new ComboBox(ComboBox::DisplayType_Simple);

            settingsLayout->AddChild(this->categoryList);
            settingsLayout->SetFlexibleChild(this->categoryList);
            settingsLayout->SetDefaultChildFill(true);
            settingsLayout->SetSizeConstraints(LayoutFillParent, LayoutFillParent);
        }
        this->mainLayout->AddChild(settingsLayout);
        this->mainLayout->SetFlexibleChild(settingsLayout);

        // Row 2
        LinearLayout* buttonLayout = new LinearLayout(LinearColumnLayout);
        buttonLayout->SetDefaultChildFill(true);
        {
            this->okButton = buttonLayout->AddChild(new Button(_T("OK")));
            this->cancelButton = buttonLayout->AddChild(new Button(_T("Cancel")));
        }
        this->mainLayout->AddChild(buttonLayout);
        this->mainLayout->SetChildAlignment(buttonLayout, ChildAlignRight);
    }
    this->AddChild(this->mainLayout);
}

void PreferencesView::OnMainWindowResized(Window* window, win32cpp::Size newSize)
{
    this->Resize(newSize);
}