//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2008, Casey Langen, Andr� W�sten
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
 * @file CheckBox.hpp
 * @brief Standard check box control.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. CheckBox wraps the
 * Win32 BUTTON control in its BS_AUTOCHECKBOX (or custom) mode and exposes
 * checked, unchecked and indeterminate states through the CheckboxPressedEvent
 * signal and helper methods.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Window.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

class CheckBox;

/** @brief Signal type emitted when a CheckBox is pressed.
 *  @param sender the CheckBox that was pressed
 *  @param state the new check state (Win32 BST_* value)
 *  @see CheckBox */
typedef sigslot::signal2<CheckBox*, int> CheckboxPressedEvent;

/** @brief A standard CheckBox.
 *  @details Wraps the Win32 BUTTON control configured with a check-box
 *           style. Supports the three check states: checked, unchecked and
 *           indeterminate. */
class CheckBox : public Window
{
private: // types
    typedef Window base;

public: // events
    /** @brief Emitted when the user presses the CheckBox. */
    CheckboxPressedEvent  Pressed;

public: // constructors
    /** @brief Constructs a check box.
     *  @param caption the text displayed next to the box
     *  @param layoutFlags layout flags used for sizing
     *  @param style the Win32 button style (defaults to BS_AUTOCHECKBOX) */
    /*ctor*/            CheckBox(
        const uichar* caption = _T(""),
        LayoutFlags layoutFlags = LayoutWrapWrap,
        int style = BS_AUTOCHECKBOX);

protected: // methods
    /** @brief Creates the underlying HWND. */
    virtual HWND        Create(Window* parent);
    /** @brief Processes window messages, tracking state changes. */
    virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    /** @brief Emits the Pressed signal with the new state.
     *  @param state the new check state (Win32 BST_* value) */
    virtual void        OnPressed(int state);
    /** @brief Paints the check box. */
    virtual void        PaintToHDC(HDC hdc, const Rect& rect);

public:
    /** @brief Returns whether the check box is checked.
     *  @return true if checked */
    virtual bool        IsChecked(void) const;
    /** @brief Returns whether the check box is unchecked.
     *  @return true if unchecked */
    virtual bool        IsUnchecked(void) const;
    /** @brief Returns whether the check box is indeterminate.
     *  @return true if indeterminate */
    virtual bool        IsIndeterminate(void) const;
    /** @brief Sets the check box to the checked state. */
    virtual void        Check(void);
    /** @brief Sets the check box to the unchecked state. */
    virtual void        Uncheck(void);
    /** @brief Sets the check box to the indeterminate state. */
    virtual void        SetIndeterminate(void);

protected: // instance data
    int state;     /**< current check state (BST_* value) */
    int style;     /**< Win32 button style */
    uistring caption; /**< the check box's caption text */
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
