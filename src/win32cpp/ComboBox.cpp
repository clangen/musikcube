//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Casey Langen, André Wösten
//
// Sources and Binaries of: win32cpp
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
#include <win32cpp/ComboBox.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

ComboBox::ModelRef ComboBox::sNullModel(new ComboBox::NullModel());

ComboBox::ComboBox() :
model(ComboBox::sNullModel)
{
}


HWND ComboBox::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance();
    INITCOMMONCONTROLSEX icex;

    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_USEREX_CLASSES;

    // We need the extended combobox from the common controls
    InitCommonControlsEx(&icex);

    /**
     * TODO: For CBS_DROPDOWN-support (editable entries) pszText of COMBOBOXEXITEM in OnDataChanged
     *       must be adapted to a send/retrieve behaviour or we must implement a
     *       callback behaviour with CBEN_GETDISPINFO. (as it is in the ListView)
     */
    DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST;

    // Create the ComboBox
    HWND hwnd = CreateWindowEx(
        NULL,                       // ExStyle
        WC_COMBOBOXEX,              // Class name
        _T(""),                     // Window name
        style,                      // Style
        0,                          // X
        0,                          // Y
        0,                          // Width
        100,                        // Height
        parent->Handle(),           // Parent
        NULL,                       // Menu
        hInstance,                  // Instance
        NULL);                      // lParam

    return hwnd;
}


void ComboBox::SetModel(ModelRef model)
{
    // Unbind event handlers, assign new model and rebind event handler
    this->model->DataChanged.disconnect(this);
    this->model = model;
    this->model->DataChanged.connect(this, &ComboBox::OnDataChanged);

    // On change of model ask combobox to reload the items from the model
    this->OnDataChanged();
}


int ComboBox::Selected()
{
    return (int)this->SendMessage(
        CB_GETCURSEL,
        0,
        0
    );
}

void ComboBox::Select(int index)
{
    // If index is -1 the selection is cleared
    this->SendMessage(
        CB_SETCURSEL,
        (WPARAM)index,
        0
    );
}

void ComboBox::OnDataChanged()
{
    // Delete old items in combobox
    int last, item = 0;
    do {
        last = (int)this->SendMessage(
            CBEM_DELETEITEM,
            (WPARAM)(DWORD)item,
            0
        );
        item++;
    } while(last != 0 && last != CB_ERR);

    // Set image list (given by model)
    ImageList* imagelist = this->model->ImageList();
    if(imagelist) {
        this->SendMessage(
            CBEM_SETIMAGELIST,
            0,
            (LPARAM)imagelist->Handle()
        );
    }

    // Iterate through available items
    for(int i=0; i<this->model->ItemCount(); i++) {
        // Get item string
        uistring val = this->model->ItemToString(i);
        // Get extended data for item
        LPARAM data = this->model->ItemToExtendedData(i);
        // Get indentation for item (1 space = 10 pixels)
        int indent = this->model->ItemToIndent(i);
        // Get image for item (only valid if image list is set)
        int ilindex = this->model->ItemToImageListIndex(i);

        // Initialize combobox item
        COMBOBOXEXITEM item;

        ::ZeroMemory(&item, sizeof(COMBOBOXEXITEM));

        item.mask = CBEIF_TEXT | CBEIF_LPARAM | CBEIF_INDENT;
        item.iItem = i;
        item.pszText = (wchar_t*)val.c_str();
        item.cchTextMax = (int)val.length();
        item.lParam = data;
        item.iIndent = indent;

        if(imagelist) {
            // If given, add image from image list to item
            item.mask |= CBEIF_IMAGE | CBEIF_SELECTEDIMAGE;
            item.iImage = ilindex;
            item.iSelectedImage = ilindex;
        }

        // Insert item
        this->SendMessage(
            CBEM_INSERTITEM,
            0,
            (LPARAM)(PCOMBOBOXEXITEM)&item
        );
    }
}