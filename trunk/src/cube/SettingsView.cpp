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
#include <cube/SettingsView.hpp>
#include <win32cpp/LinearLayout.hpp>
#include <win32cpp/Button.hpp>
#include <win32cpp/CheckBox.hpp>
#include <win32cpp/RadioButton.hpp>
#include <win32cpp/GroupBox.hpp>
#include <win32cpp/ListView.hpp>
#include <win32cpp/Label.hpp>
#include <win32cpp/ComboBox.hpp>
#include <win32cpp/ImageList.hpp>


//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

class ComboBoxTestModel : public ComboBox::Model
{
private:
    std::vector<uistring> list;
    win32cpp::ImageList* il;
public: 
    ComboBoxTestModel() {
        il = new win32cpp::ImageList(16, 16, win32cpp::ImageList::Color32);
        il->Add(_T("resources\\test_combobox.bmp"));

        list.push_back(_T("Hello"));
        list.push_back(_T("this"));
        list.push_back(_T("is"));
        list.push_back(_T("a"));
        list.push_back(_T("test"));
    }

    virtual win32cpp::ImageList* ImageList() 
    {
        return il;
    }

    virtual int ItemCount()
    {
        return 5;
    }

    virtual int ItemToImageListIndex(int index)
    {
        return 0;
    }

    virtual int ItemToIndent(int index)
    {
        return 0;
    }

    virtual uistring ItemToString(int index)
    {
        return list[index];
    }
    
    virtual LPARAM ItemToExtendedData(int index)
    {
        return 0;
    }
};

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    SettingsView::SettingsView()
{
}


void        SettingsView::OnPressTestCheckbox(CheckBox* CheckBox , int state)
{
    if(state == BST_CHECKED) {
        ::MessageBox(NULL, _T("checked"), _T("checked"), MB_OK);
    } else if(state == BST_UNCHECKED) {
        ::MessageBox(NULL, _T("unchecked"), _T("unchecked"), MB_OK);
    } else if(state == BST_INDETERMINATE) {
        ::MessageBox(NULL, _T("indeterminate"), _T("indeterminate"), MB_OK);
    }
}

void        SettingsView::OnCreated()
{

    LinearLayout* mainLayout = new LinearLayout(LinearRowLayout);
    LinearLayout* pathLayout = new LinearLayout(LinearColumnLayout);
    LinearLayout* pathButtonsLayout = new LinearLayout(LinearRowLayout);
    LinearLayout* libraryStatusLayout = new LinearLayout(LinearColumnLayout);
    

    // Library status
    FontRef boldFont(Font::Create());
    boldFont->SetBold(true);

    win32cpp::Label *status = libraryStatusLayout->AddChild(new Label(_(_T("Library status"))));
    status->SetFont(boldFont);
    this->libraryStatus     = libraryStatusLayout->AddChild(new Label());

    // Path list

    this->pathList          = pathLayout->AddChild(new ListView());
/*    win32cpp::ListView::ColumnRef pathColumn = ListView::Column::Create(_T("Path"),1000);
    this->pathList->AddColumn(pathColumn);
    this->pathList->SetScrollBarVisibility(win32cpp::ScrollBar::HorizontalScrollBar,false);
*/
    pathLayout->SetSizeConstraints(LayoutFillParent,120);
    pathLayout->SetFlexibleChild(this->pathList);
    pathLayout->SetDefaultChildFill(true);
    pathLayout->SetDefaultChildAlignment(ChildAlignRight);


    // pathButtons layout

    this->addPathButton     = pathButtonsLayout->AddChild(new Button(_(_T("Add path"))));
    this->removePathButton  = pathButtonsLayout->AddChild(new Button(_(_T("Remove path"))));

    this->addPathButton->Resize(90, 24);
    this->removePathButton->Resize(90, 24);

    pathButtonsLayout->SetDefaultChildFill(false);
    pathButtonsLayout->SetDefaultChildAlignment(ChildAlignMiddle);
    pathButtonsLayout->SetSizeConstraints(90,LayoutFillParent);

    pathLayout->AddChild(pathButtonsLayout);

    // Add to the layout
    mainLayout->AddChild(new Frame(libraryStatusLayout,FramePadding(20,20,20,0)));
    mainLayout->AddChild(new Frame(pathLayout,FramePadding(20,20,0,20)));

    // test CheckBox
    CheckBox* c = new CheckBox(_T("Test 1"));
    mainLayout->AddChild(c);
    c->Check();
    c->Pressed.connect(this, &SettingsView::OnPressTestCheckbox);
    
    CheckBox* c2 = new CheckBox(_T("Test 2"), BS_AUTO3STATE);
    mainLayout->AddChild(c2);
    c2->SetIndeterminate();
    c2->Pressed.connect(this, &SettingsView::OnPressTestCheckbox);

    // test radio buttons
    RadioButton* r11 = new RadioButton(_T("Group 1: Button A"));
    mainLayout->AddChild(r11);
    
    RadioButton* r12 = new RadioButton(_T("Group 1: Button B"), r11);
    mainLayout->AddChild(r12);
    
    RadioButton* r13 = new RadioButton(_T("Group 1: Button C"), r12);
    mainLayout->AddChild(r13);
    
    RadioButton* r21 = new RadioButton(_T("Group 2: Button A"));
    mainLayout->AddChild(r21);
    
    RadioButton* r22 = new RadioButton(_T("Group 2: Button B"), r21);
    mainLayout->AddChild(r22);
    
    RadioButton* r23 = new RadioButton(_T("Group 2: Button C"), r22);
    mainLayout->AddChild(r23);

    r11->Check(); r12->Check(); r13->Check();
    r21->Check(); r22->Check();

    RadioButton* checked = r11->GetCheckedInGroup();
    //if(checked) ::MessageBox(NULL, checked->Caption().c_str(), _T("test"), MB_OK);
    checked = r22->GetCheckedInGroup();
    //if(checked) ::MessageBox(NULL, checked->Caption().c_str(), _T("test"), MB_OK);

    // test groupbox
    GroupBox* g = new GroupBox(_T("test group box"));
    CheckBox* gc = new CheckBox(_T("Test CheckBox"));
    CheckBox* gc2 = new CheckBox(_T("Test CheckBox2"));
    CheckBox* gc3 = new CheckBox(_T("Test CheckBox2"));

    g->AddChild(gc);
    g->AddChild(gc2);
    g->AddChild(gc3);

    gc2->MoveRelativeTo(0, 20);
    gc3->MoveRelativeTo(0, 40);
    
    mainLayout->AddChild(g);

    // test combobox
    ComboBox* cb = new ComboBox;
    ComboBox::ModelRef cb_testmodel(new ComboBoxTestModel);

    mainLayout->AddChild(cb);    
    cb->SetModel(cb_testmodel);
    cb->Select(2);

    this->AddChild(mainLayout);

}
