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
#include <win32cpp/Win32Config.hpp>
#include <win32cpp/EditView.hpp>
#include <win32cpp/Application.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

///\brief
///Constructor.
///
///\param width
///The width of the edit box
///\param height
///The height of the edit box
/*ctor*/    EditView::EditView(int width, int height)
: base()
, width(width)
, height(height)
, caption(caption)
{
}

/*ctor*/    EditView::EditView(const uistring& caption)
: base()
, width(200)
, height(22)
, caption(caption)
{
}

EditView::~EditView()
{
}

HWND        EditView::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance();

    // create the window
    DWORD style = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
    DWORD styleEx = WS_EX_CLIENTEDGE;
    //
    HWND hwnd = CreateWindowEx(
        styleEx,                // ExStyle
        _T("EDIT"),             // Class name
        NULL,                   // Window name
        style,                  // Style
        0,                      // X
        0,                      // Y
        this->width,            // Width
        this->height,           // Height
        parent->Handle(),       // Parent
        NULL,                   // Menu
        hInstance,              // Instance
        NULL);                  // lParam

    if (hwnd)
    {
        ::SetWindowText(hwnd, this->caption.c_str());
    }

    return hwnd;
}

LRESULT     EditView::PreWindowProc(UINT message, WPARAM wParam, LPARAM lParam, bool &discardMessage)
{
    switch (message)
    {
    case WM_CHAR:
        {
            // avoid a *ding* that chimes if we don't say we want tab.
            if ((wParam == VK_TAB) && (this->TabStop()))
            {
                this->SelectAll();
                discardMessage = true;
            }
        }
        break;
    }

    return 0;
}

LRESULT     EditView::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            WORD notifyHeader = HIWORD(wParam);
            switch (notifyHeader)
            {
            case EN_CHANGE:
                this->OnChanged();
                return 0; // 0 = processed
            }
        }
        break;
    }

    return this->DefaultWindowProc(message, wParam, lParam);
}

void EditView::OnChanged()
{
    this->Changed(this);
}

///\brief SetReadOnly
///Sets the edit control to be read only.
///\param setting
///True or False (Read only and Read/Write respectively)
void        EditView::SetReadOnly(bool setting)
{
    Edit_SetReadOnly(this->Handle(), setting);
}

///\brief ShowCaret
///Moves the caret to a visible position.
///
void        EditView::ShowCaret()
{
    Edit_ScrollCaret(this->Handle());
    return;
}

///\brief SetTooltip
///Sets the tooltip text for the edit control.
///\param text
///The tooltip text to set
void        EditView::SetTooltip(uistring text)
{
    Edit_SetCueBannerText(this->Handle(), text.c_str());
    //Edit_SetCueBannerTextFocused(this->headerHandle, text.c_str(), true);
    //The second one is vista only
}

///\brief LimitText
///Sets the limit for the amount of text that can be entered
///\param chars
///The number of characters allowed
void        EditView::LimitText(int chars)
{
    Edit_LimitText(this->Handle(), chars);
}

///\brief SetSelection
///Sets the positions of the first and last character of selected text in the edit box
///\param first
///The position of the first char to select
///\param second
///The position of the end char to select up to
void            EditView::SetSelection(int first, int second)
{
    Edit_SetSel(this->Handle(), first, second);
}

///\brief SelectAll
///Selects all the characters in the view
void            EditView::SelectAll()
{
    Edit_SetSel(this->Handle(), 0, -1);
}

///\brief Undo
///Undoes the last edit
///
void        EditView::Undo()
{
    if(Edit_CanUndo(this->Handle()))
    {
        Edit_Undo(this->Handle());
    }

    return;
}
