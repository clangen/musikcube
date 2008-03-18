//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
//
// Sources and Binaries of: mC2, win32cpp
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
#include <win32cpp/Color.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

///\brief
///Constructor.
///
///\param red
///The red component of the color.
///
///\param green
///The green component of the color.
///
///\param blue
///The blue component of the color.
///
///\param alpha
///The alpha component of the color.
/*ctor*/    Color::Color(byte red, byte green, byte blue, byte alpha)
: red(red)
, green(green)
, blue(blue)
, alpha(alpha)
{
}

///\brief
///Constructor.
///
///\param color
///A COLORREF used to create this color
/*ctor*/    Color::Color(COLORREF color)
: red(GetRValue(color))
, green(GetGValue(color))
, blue(GetBValue(color))
, alpha(0)
{
}

byte        Color::ClampByte(int value)
{
    if (value > 255) return 255;
    if (value < 0) return 0;
    return value;
}

///\brief
///Returns a Color based on a system color identifier.
///
///For example:
///\code
///Color myColor = Color::SystemColor(COLOR_BTNFACE);
///\endcode
///
///\param systemColorID
///A system color identifier.
///
///\returns
///A Color based on the specified system color identifier.
Color       Color::SystemColor(DWORD systemColorID)
{
    COLORREF sysColor = ::GetSysColor(systemColorID);
    return Color(
        GetRValue(sysColor),
        GetGValue(sysColor),
        GetBValue(sysColor));
}

///\brief
///Lightens a Color by the specified amount.
///
///\param color
///The color to lighten.
///
///\param amount
///The amount to lighten.
///
///\returns
///A new Color, "amount" lighter than the input color.
///
///\see
///Color::Darken
Color       Color::Lighten(const Color& color, byte amount)
{
    return Color(
        ClampByte(color.red + amount),
        ClampByte(color.green + amount),
        ClampByte(color.blue + amount),
        color.alpha);
}

///\brief
///Darkens a Color by the specified amount.
///
///\param color
///The color to darken.
///
///\param amount
///The amount to darken.
///
///\returns
///A new Color, "amount" darker than the input color.
///
///\see
///Color::Lighten
Color       Color::Darken(const Color& color, byte amount)
{
    return Color(
        ClampByte(color.red - amount),
        ClampByte(color.green - amount),
        ClampByte(color.blue - amount),
        color.alpha);
}

///\brief
///Returns a Win32 COLORREF based on the Color.
Color::operator COLORREF()
{
    return RGB(this->red, this->green, this->blue);
}

//////////////////////////////////////////////////////////////////////////////

} // win32cpp