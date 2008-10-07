//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
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

ComboBox::ComboBox()
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

    DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWN;

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
    this->model->ItemCountChanged.disconnect(this);
    this->model->DataChanged.disconnect(this);
    //
    this->model = model;
    //
    this->model->ItemCountChanged.connect(this, &ComboBox::OnItemCountChanged);
    this->model->DataChanged.connect(this, &ComboBox::OnDataChanged);
}
