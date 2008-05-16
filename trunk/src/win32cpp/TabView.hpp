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

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Window.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

///\brief
///Panel is the most basic concrete implementation of Container.
///
///Panel does not offer any special layout functionality, and has no
///limitations as to the number of child controls that can be added.
///Panel is the base class for most more advanced Container
///implementations, including Splitter and BoxLayout.
///
///\see
///BoxLayout, Splitter
class TabView: public Container, public ILayout
{
public: // types
    typedef Container base;
    class TabCreationFailedException: public Exception { };
    class AddTabNotUsedException: public Exception { };

private: // types
    typedef std::map<Window*, int> WindowToTabIndexMap;

public: // constructors
    /*ctor*/        TabView();

public: // methods
    template <typename WindowType>
    WindowType* AddTab(const uistring& tabTitle, WindowType* window);
    
    template <typename WindowType>
    WindowType* RemoveTab(WindowType* window);

protected: // methods
    virtual HWND    Create(Window* parent);
    virtual void    OnPaint();
    virtual void    OnEraseBackground(HDC hdc);
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void    OnTabSelected();
    virtual void    Layout();
    virtual void    OnResized(const Size& newSize);
    virtual void    OnChildAdded(Window* child);

    Window*         WindowForTabIndex(int tabIndex);
    void            SelectFirstChild();

protected: // instance data
    WindowToTabIndexMap windowToTabMap;
    Window* visibleChild;
};

//////////////////////////////////////////////////////////////////////////////

template <typename WindowType>
WindowType* TabView::AddTab(const uistring& tabTitle, WindowType* window)
{
    this->windowToTabMap[window] = (int) this->windowToTabMap.size(); // placeholder
    this->AddChild(window);

    TCITEM tabItem;
    ::SecureZeroMemory(&tabItem, sizeof(tabItem));
    tabItem.iImage = -1;
    tabItem.mask = TCIF_TEXT | TCIF_IMAGE;
    tabItem.pszText = const_cast<wchar_t*>(tabTitle.c_str());
    
    int result = TabCtrl_InsertItem(this->Handle(), this->windowToTabMap.size(), &tabItem);
    if (result == -1)
    {
        this->windowToTabMap.erase(window);
        throw TabCreationFailedException();
    }
    else
    {
        // make sure this is still correct...
        this->windowToTabMap[window] = result;
    }

    if (this->windowToTabMap.size() == 1)
    {
        this->SelectFirstChild();
    }

    return window;
}

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
