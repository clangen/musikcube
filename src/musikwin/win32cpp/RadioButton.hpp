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
 * @file RadioButton.hpp
 * @brief Standard radio button control.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. RadioButton wraps
 * the Win32 BUTTON control in radio mode. Radio buttons are organised as a
 * linked list to enforce group behaviour: only one button in the group may
 * be checked at a time.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Window.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

class RadioButton;

/** @brief Signal type emitted when a RadioButton is pressed.
 *  @see RadioButton */
typedef sigslot::signal1<RadioButton*> RadioButtonPressedEvent;

/** @brief A standard RadioButton.
 *  @details Radio buttons are organised as a linked list to enforce the
 *           grouping behaviour: checking one button unchecks the others in
 *           the same group. */
class RadioButton : public Window
{
private: // types
    typedef Window base;

public: // events
    /** @brief Emitted when the user presses the RadioButton. */
    RadioButtonPressedEvent  Pressed;

public: // constructors
    /** @brief Constructs a radio button, optionally attached to a group.
     *  @param caption the text displayed next to the radio
     *  @param attach a sibling radio button to form a group with
     *  @param layoutFlags layout flags used for sizing */
    /*ctor*/            RadioButton(
        const uichar* caption = _T(""),
        RadioButton* attach = NULL,
        LayoutFlags layoutFlags = LayoutWrapWrap);

    /** @brief Constructs a radio button.
     *  @param caption the text displayed next to the radio
     *  @param layoutFlags layout flags used for sizing */
    /*ctor*/            RadioButton(
        const uichar* caption,
        LayoutFlags layoutFlags = LayoutWrapWrap);

    /** @brief Destroys the radio button. */
    /*dtor*/            ~RadioButton();

protected: // methods
    /** @brief Creates the underlying HWND. */
    virtual HWND        Create(Window* parent);
    /** @brief Processes window messages, updating group state. */
    virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    /** @brief Emits the Pressed signal. */
    virtual void        OnPressed();
    /** @brief Paints the radio button. */
    virtual void        PaintToHDC(HDC hdc, const Rect& rect);

public:
    /** @brief Checks this radio button, unchecking the rest of the group. */
    void                Check(void);
    /** @brief Returns whether this radio button is checked.
     *  @return true if checked */
    bool                IsChecked(void);

    /** @brief Returns the checked radio button in this group.
     *  @return pointer to the checked button, or NULL */
    RadioButton*        GetCheckedInGroup(void);

    /** @brief Returns the caption text (used for testing).
     *  @return the caption string */
    uistring            Caption(void) const { return caption; }

protected: // instance data
    RadioButton* prev;  /**< previous button in the group, NULL if first */
    RadioButton* next;  /**< next button in the group, NULL if last */
    uistring caption;   /**< the radio button's caption text */
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
