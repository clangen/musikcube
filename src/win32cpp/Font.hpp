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

#include <win32cpp/Types.hpp>
#include <win32cpp/Exception.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// Font
//////////////////////////////////////////////////////////////////////////////

struct Font;

///
///\brief A shared pointer to a Font object
///
typedef boost::shared_ptr<Font> FontRef;

///\brief
///TextAlignment specifies how text should be aligned when rendered.
///
///\see
///Font::DrawToHDC
enum TextAlignment
{
    TextAlignLeft = DT_LEFT,        /*!< */ 
    TextAlignCenter = DT_CENTER,    /*!< */ 
    TextAlignRight = DT_RIGHT       /*!< */ 
};

///\brief
///Font provides a light-weight wrapper around a Win32 HFONT object.
///
///Font exposes many useful utility functions for manipulating the underlying
///font attributes, including: face, size, weight, italic, and underline.
///
///\see
///FontRef
struct Font
{
public: // types
    class InvalidFontWeightException: public Exception { };

private: // constructors
    /*ctor*/    Font();

    /*ctor*/    Font(const uistring& face, unsigned pointSize = -1, 
                bool bold = false, bool italic = false, bool underline = false);

    /*ctor*/    Font(const LOGFONT& logFont, HDC hdc = NULL);


public: // creation methods
    static FontRef Create() 
    {
        return FontRef(new Font());
    }

    static FontRef Create(const uistring& face, int pointSize = -1, 
    bool bold = false, bool italic = false, bool underline = false)
    {
        return FontRef(new Font(face, pointSize, bold, italic, underline));
    }


    static FontRef Create(const LOGFONT& logFont, HDC hdc = NULL)
    {
        return FontRef(new Font(logFont, hdc));
    }

public: // destructor
    /*dtor*/    ~Font();

public: // methods
    void        DrawToHDC(HDC hdc, const Rect& rect, const uistring& caption, TextAlignment = TextAlignLeft);
    uistring    FaceName() const;
    void        SetFaceName(const uistring& faceName);
    unsigned    PointSize() const;
    void        SetPointSize(unsigned pointSize);
    bool        Bold() const;
    void        SetBold(bool bold = true);
    int         Weight() const;
    void        SetWeight(int weight);
    bool        Italic() const;
    void        SetItalic(bool italic = true);
    bool        Underline() const;
    void        SetUnderline(bool underline = true);
    HFONT       GetHFONT();

    static HFONT  CreateHFONT(const Font& font, HDC hdc);

protected: // methods
    void    Invalidate() { this->invalid = true; }
    bool    InitializeFont(HDC hdc);
    bool    InitializeFont(const LOGFONT& font, HDC hdc);

private: // instance data
    uistring faceName;
    unsigned pointSize;
    bool italic, underline;
    int weight;
    bool invalid;
    HFONT font;
};

//////////////////////////////////////////////////////////////////////////////

} // win32cpp