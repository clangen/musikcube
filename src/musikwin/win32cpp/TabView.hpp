//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2007, Casey Langen
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

/**
 * @file TabView.hpp
 * @brief Tabbed container control.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. TabView wraps the
 * Win32 tab control (SysTabControl32) to present one child window at a time,
 * selectable through tabs. Only the active child is visible; the others are
 * hidden until their tab is selected.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Container.hpp>
#include <win32cpp/ILayout.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief A tabbed container control.
 *  @details Wraps the Win32 tab control. Children are registered as tabs
 *           with AddTab(); only the active tab's window is visible. The
 *           ActiveWindow(), SetActiveTab() and Padding() methods manage the
 *           current tab and its content area.
 *  @see BoxLayout, Splitter */
class TabView: public Container, public ILayout
{
public: // types
    typedef Container base;
    class TabCreationFailedException: public Exception { }; /**< tab could not be inserted */
    class AddTabNotUsedException: public Exception { };     /**< child was added, not tabbed */

private: // types
    typedef std::map<Window*, int> WindowToTabIndexMap;

public: // constructors
    /** @brief Constructs a tab view.
     *  @param layoutFlags layout flags used for sizing */
    /*ctor*/        TabView(LayoutFlags layoutFlags = LayoutWrapWrap);

public: // methods
    /** @brief Adds a child window as a new tab.
     *  @tparam WindowType the concrete window type
     *  @param tabTitle the text shown on the tab
     *  @param window the child window to tab
     *  @return the added window */
    template <typename WindowType>
    WindowType* AddTab(const uistring& tabTitle, WindowType* window);

    /** @brief Removes a tabbed child window.
     *  @tparam WindowType the concrete window type
     *  @param window the child window to remove
     *  @return the removed window */
    template <typename WindowType>
    WindowType* RemoveTab(WindowType* window);

    /** @brief Returns the currently active tab's window.
     *  @return the active window, or NULL */
    Window* ActiveWindow();
    /** @brief Returns the client area size below the tabs.
     *  @return the usable client size */
    virtual Size ClientSize() const;
    /** @brief Returns the padding around the content area.
     *  @return the padding in pixels */
    int Padding() const;
    /** @brief Sets the padding around the content area.
     *  @param padding the padding in pixels */
    void SetPadding(int padding);
    /** @brief Activates the tab holding the given window.
     *  @param window the window to activate */
    void SetActiveTab(Window* window);
    /** @brief Activates the tab at the given index.
     *  @param index the tab index */
    void SetActiveTab(unsigned index);

protected: // methods
    virtual HWND    Create(Window* parent);
    virtual void    OnPaint();
    virtual void    OnEraseBackground(HDC hdc);
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void    OnTabSelected();
    virtual void    Layout();
    virtual void    OnResized(const Size& newSize);
    virtual void    OnChildAdded(Window* child);
    virtual LRESULT DrawItem(DRAWITEMSTRUCT& item);
    Window*         WindowForTabIndex(int tabIndex);
    void            SelectFirstChild();

protected: // instance data
    WindowToTabIndexMap windowToTabMap; /**< maps children to tab indices */
    Window* visibleChild;               /**< the currently visible child */
    int padding;                        /**< content area padding */
};

//////////////////////////////////////////////////////////////////////////////

/** @brief Adds a child window as a new tab.
 *  @tparam WindowType the concrete window type
 *  @param tabTitle the text shown on the tab
 *  @param window the child window to tab
 *  @return the added window */
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
