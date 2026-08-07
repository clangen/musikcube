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
 * @file EditView.hpp
 * @brief Single-line text edit control.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. EditView wraps the
 * Win32 EDIT control, providing text entry with change notifications
 * (EditViewChangedEvent), read-only mode, tooltips, selection control and
 * undo support.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Window.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

class EditView; // forward decl

/** @brief Signal type emitted when the edit control's text changes.
 *  @see EditView */
typedef sigslot::signal1<EditView*> EditViewChangedEvent;

/** @brief A standard single-line edit control.
 *  @details Wraps the Win32 EDIT control. The Changed signal is emitted
 *           whenever the user modifies the text; the control also supports
 *           caret display, tooltips, a character limit, text selection and
 *           undo. */
class EditView: public Window
{
private:    // typedefs
    typedef Window base;

public:     // events
    /** @brief Emitted when the contents of the edit control change. */
    EditViewChangedEvent    Changed;

public:     // constructors, methods
    /** @brief Constructs an edit control with a fixed size.
     *  @param width the initial width in pixels
     *  @param height the initial height in pixels */
    /*ctor*/        EditView(int width, int height);
    /** @brief Constructs an edit control with layout flags.
     *  @param layoutFlags layout flags used for sizing */
    /*ctor*/        EditView(LayoutFlags layoutFlags);
    /** @brief Constructs an edit control with initial text.
     *  @param caption the initial text
     *  @param layoutFlags layout flags used for sizing */
    /*ctor*/        EditView(const uistring& caption = _T(""), LayoutFlags layoutFlags = LayoutWrapWrap);
    /** @brief Destroys the edit control. */
    /*dtor*/        ~EditView();

    /** @brief Enables or disables editing.
     *  @param setting true for read-only, false to allow editing */
    void    SetReadOnly(bool setting);
    /** @brief Shows the text caret. */
    void    ShowCaret();
    /** @brief Sets a tooltip for the control.
     *  @param text the tooltip text */
    void    SetTooltip(uistring text);
    /** @brief Limits the number of characters the user can type.
     *  @param chars the maximum number of characters */
    void    LimitText(int chars);
    /** @brief Selects a range of characters.
     *  @param first the index of the first selected character
     *  @param second the index one past the last selected character */
    void    SetSelection(int first, int second);
    /** @brief Selects all of the text. */
    void    SelectAll();

    /** @brief Undoes the last edit operation. */
    void    Undo();

protected:  // methods
    /** @brief Creates the underlying HWND. */
    virtual HWND       Create(Window* parent);
    /** @brief Pre-processes messages, intercepting WM_CHAR handling. */
    virtual LRESULT    PreWindowProc(UINT message, WPARAM wParam, LPARAM lParam, bool &discardMessage);
    /** @brief Processes window messages. */
    virtual LRESULT    WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    /** @brief Emits the Changed signal. */
    virtual void       OnChanged();

private:
    void InitializeInstance();

protected:  // instance data
    uistring caption;   /**< initial text of the control */
    int width;          /**< initial width in pixels */
    int height;         /**< initial height in pixels */
    uistring editText;  /**< current text of the control */
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
