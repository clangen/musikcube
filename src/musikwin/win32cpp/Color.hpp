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
 * @file Color.hpp
 * @brief RGBA color value with Win32 COLORREF interop.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Color stores a
 * red/green/blue/alpha quad and is implicitly convertible to and from a
 * Win32 COLORREF so it can be used directly with GDI drawing routines.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Types.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief An RGBA color.
 *  @details This class can be implicitly cast to and from Win32 COLORREF
 *           objects. Alpha is stored separately and defaults to 0.
 *  @code
 *  COLORREF baseColor = RGB(255, 0, 0);
 *  Color myColor = baseColor;
 *  Color::Lighten(myColor, 15);
 *  COLORREF newColor = myColor;
 *  @endcode */
struct Color
{
public: // constructors
    /** @brief Constructs a color from its components.
     *  @param red the red component (0-255)
     *  @param green the green component (0-255)
     *  @param blue the blue component (0-255)
     *  @param alpha the alpha component (0-255) */
    /*ctor*/    Color(byte red = 255, byte green = 255, byte blue = 255, byte alpha = 0);
    /** @brief Constructs a color from a Win32 COLORREF.
     *  @param color the COLORREF to convert */
    /*ctor*/    Color(COLORREF color);

public: // fields
    byte red;   /**< red component (0-255) */
    byte green; /**< green component (0-255) */
    byte blue;  /**< blue component (0-255) */
    byte alpha; /**< alpha component (0-255) */

public: // methods
    /** @brief Clamps an integer into the valid byte range (0-255).
     *  @param value the value to clamp
     *  @return the clamped byte value */
    static byte ClampByte(int value);
    /** @brief Returns a color for the given Win32 system color ID.
     *  @param systemColorID a GetSysColor() color identifier
     *  @return the corresponding system color */
    static Color SystemColor(DWORD systemColorID);
    /** @brief Returns a lightened copy of the given color.
     *  @param color the base color
     *  @param amount how much to lighten (0-255)
     *  @return the lightened color */
    static Color Lighten(const Color& color, byte amount);
    /** @brief Returns a darkened copy of the given color.
     *  @param color the base color
     *  @param amount how much to darken (0-255)
     *  @return the darkened color */
    static Color Darken(const Color& color, byte amount);

public: // operators
    /** @brief Converts the color to a Win32 COLORREF. */
    virtual operator COLORREF();
};

//////////////////////////////////////////////////////////////////////////////

} // win32cpp
