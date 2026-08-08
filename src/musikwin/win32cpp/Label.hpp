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
 * @file Label.hpp
 * @brief Simple text label control.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Label is a Window
 * that draws only a caption. It can optionally resize itself to fit the
 * caption's text when the font or caption changes.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Window.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief A simple Window that draws only a caption. */
class Label: public Window
{
private: // types
    typedef Window base;

public: // constructors
    /** @brief Constructs a label with the given text.
     *  @param caption the text to display */
    /*ctor*/            Label(const uichar* caption = _T(""));

public: // methods
    /** @brief Resizes the label to fit its current caption. */
    void    ResizeFromCaption();
    /** @brief Enables or disables auto-resizing on caption changes.
     *  @param enable true to resize automatically */
    void    EnableAutoResizeFromCaption(bool enable = true);
    /** @brief Returns whether auto-resizing is enabled.
     *  @return true if auto-resize is enabled */
    bool    AutoResizeFromCaptionEnabled() const;

protected: // methods
    /** @brief Creates the underlying HWND. */
    virtual HWND        Create(Window* parent);
    /** @brief Processes window messages. */
    virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    /** @brief Resizes the label when the font changes. */
    virtual void        OnFontChanged();
    /** @brief Resizes the label when the caption changes. */
    virtual void        OnCaptionChanged();

protected: // instance data
    uistring caption;   /**< the label's text */
    bool autoResize;    /**< whether to auto-resize on text changes */
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
