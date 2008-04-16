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
#include <win32cpp/Font.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////
// Font
//////////////////////////////////////////////////////////////////////////////

#define INVALIDATE_FONT_ATTRIBUTE(varName)  \
            this->varName = varName;        \
            this->Invalidate();             \

///\brief
///Default constructor.
/*ctor*/    Font::Font()
: invalid(true)
, font(NULL)
, faceName(_T(""))
, pointSize(-1)
, weight(FW_REGULAR)
, italic(false)
, underline(false)
{
}

///\brief
///Constructor.
///
///\param fontFace
///the name of the fontface to use.
///
///\param pointSize
///the pointsize of the font.
///
///\param bold
///if true the font will be bold
///
///\param italic
///if true the font will be italic
///
///\param underline
///if true the font will be underlined
/*ctor*/    Font::Font(const uistring& fontFace, unsigned pointSize, bool bold, bool italic, bool underline)
: invalid(true)
, font(NULL)
, faceName(fontFace)
, pointSize(pointSize)
, weight(bold ? FW_BOLD : FW_REGULAR)
, italic(italic)
, underline(underline)
{
}

///\brief
///Constructor.
///
///\param logFont
///the font parameters to use when creating this font.
///
///\param hdc
///The device context to use when creating the HFONT. If this parameter
///is NULL the Desktop's HDC will be used.
/*ctor*/    Font::Font(const LOGFONT& logFont, HDC hdc)
: invalid(true)
{
    this->InitializeFont(logFont, hdc);
}

/*dtor*/    Font::~Font()
{
    ::DeleteObject(this->font);
}

///\brief
///Renders text to the specified device context.
///
///\param hdc
///the device context to render the text to.
///
///\param rect
///the bounding rectangle.
///
///\param caption
///the text to render.
///
///\param alignment
///the text alignment to use
void        Font::DrawToHDC(HDC hdc, const Rect& rect, const uistring& caption, TextAlignment alignment)
{
    if (this->invalid)
    {
        this->InitializeFont(hdc);
        this->invalid = false;
    }

    HGDIOBJ oldObject = ::SelectObject(hdc, this->font);

    // draw
    DRAWTEXTPARAMS drawTextParams;
    ::SecureZeroMemory(&drawTextParams, sizeof(DRAWTEXTPARAMS));
    drawTextParams.cbSize = sizeof(DRAWTEXTPARAMS);
    //
    int bufferSize = (int) caption.size() + 5;  // DrawTextEx may add up to 4 additional characters
    boost::scoped_ptr<uichar> buffer(new uichar[bufferSize]);
    ::wcsncpy_s(buffer.get(), bufferSize, caption.c_str(), caption.size());
    //
    RECT drawRect = rect;
    //
    ::DrawTextEx(
        hdc,
        buffer.get(),
        -1, // buffer is NULL terminated
        &drawRect,
        DT_END_ELLIPSIS | DT_VCENTER | DT_SINGLELINE | alignment,
        &drawTextParams);

    ::SelectObject(hdc, oldObject);
}

///\brief
///Return the HFONT representation of the Font object. Do NOT call
///::DeleteObject() on the HFONT returned.
///
///\returns
///a HFONT representation of the Font.
HFONT       Font::GetHFONT()
{
    if (this->invalid)
    {
        this->InitializeFont(NULL);
        this->invalid = false;
    }

    return this->font;
}

///\returns
///the Font's faceName.
uistring    Font::FaceName() const
{
    return this->faceName;
}

///\brief
///Sets the Font's facename.
///
///\param faceName
///the font's facename.
void        Font::SetFaceName(const uistring& faceName)
{
    INVALIDATE_FONT_ATTRIBUTE(faceName)
}

///\brief
///Returns the point size of the font.
unsigned    Font::PointSize() const
{
    return this->pointSize;
}

///\brief
///Sets the Font's point size.
void        Font::SetPointSize(unsigned pointSize)
{
    INVALIDATE_FONT_ATTRIBUTE(pointSize)
}

///\returns
///true if the Font is bold.
bool        Font::Bold() const
{
    return this->weight > 400;
}

///\brief
///Sets the Font's bold property.
void        Font::SetBold(bool bold)
{
    if ((bold) && (this->weight != FW_BOLD))
    {
        this->weight = FW_BOLD;
        this->Invalidate();
    }
    else if (( ! bold) && (this->weight != FW_REGULAR))
    {
        this->weight = FW_REGULAR;
        this->Invalidate();
    }
}

///\returns
///Returns the Font's weight.
///
///See: http://msdn2.microsoft.com/en-us/library/ms534214.aspx
int         Font::Weight() const
{
    return this->weight;
}

///\brief
///Sets the Font's weight.
///
///See: http://msdn2.microsoft.com/en-us/library/ms534214.aspx
void        Font::SetWeight(int weight)
{
    if((weight > 1000) && (weight < 0))
    {
        throw InvalidFontWeightException();
    }

    if (weight != this->weight)
    {
        this->weight = weight;
        this->Invalidate();
    }
}

///\brief
///Returns the Font's italic property.
bool        Font::Italic() const
{
    return this->italic;
}

///\brief
///Sets the Font's weight.
///
///See: http://msdn2.microsoft.com/en-us/library/ms534214.aspx
void        Font::SetItalic(bool italic)
{
    INVALIDATE_FONT_ATTRIBUTE(italic)
}

///\brief
///Returns the Font's underline property
bool        Font::Underline() const
{
    return this->underline;
}

///\brief
///Sets the Font's underline property
void        Font::SetUnderline(bool underline)
{
    INVALIDATE_FONT_ATTRIBUTE(underline)
}

bool        Font::InitializeFont(HDC hdc)
{
    HFONT newFont = Font::CreateHFONT(*this, hdc);
    if (newFont)
    {
        ::DeleteObject(this->font);
        this->font = newFont;
    }

    return (newFont != NULL);
}

bool        Font::InitializeFont(const LOGFONT& logFont, HDC hdc)
{
    bool releaseDC = ( ! hdc);
    //
    if ( ! hdc)
    {
        // if no HDC is supplied then use the desktop's
        hdc = ::GetDC(NULL);
    }

    this->pointSize = -MulDiv(logFont.lfHeight, 72, ::GetDeviceCaps(hdc, LOGPIXELSY));
    this->faceName.assign(logFont.lfFaceName);
    this->weight = logFont.lfWeight;
    this->italic = (logFont.lfItalic != 0);
    this->underline = (logFont.lfUnderline != 0);

    bool result = this->InitializeFont(hdc);

    if (releaseDC)
    {
        ::ReleaseDC(NULL, hdc);
    }

    return result;
}

///\brief
///Create a HFONT from a Font object. The caller is responsible for calling
///::DeleteObject() on the HFONT when finished with it.
///
///\param font
///the Font to use when creating the HFONT
///
///\param hdc
///the device context to use when creating the font. If this parameter is
///NULL the desktop DC will be used.
///
///\returns
///a new HFONT. Caller is responsible for calling ::DeleteObject()
HFONT       Font::CreateHFONT(const Font& font, HDC hdc)
{
    bool releaseDC = (hdc == NULL);
    if (hdc == NULL)
    {
        hdc = ::GetDC(NULL);
    }

    NONCLIENTMETRICS nonClientMetrics;
    nonClientMetrics.cbSize = sizeof(NONCLIENTMETRICS);
    ::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &nonClientMetrics, NULL);

    uistring faceName = font.FaceName();
    // if no face is specified use the system defaults.
    if ( ! faceName.size())
    {
        faceName.assign(nonClientMetrics.lfMessageFont.lfFaceName);
    }

    // if the size is -1, use default system size
    int height;
    //
    if (font.pointSize == -1)
    {
        height = nonClientMetrics.lfMessageFont.lfHeight;
    }
    else
    {
        height = -MulDiv(font.pointSize, ::GetDeviceCaps(hdc, LOGPIXELSY), 72); // from MSDN CreateFont()
    }

    HFONT newFont = ::CreateFont(
        height,
        0,                          // width. 0 = default
        0,                          // angle of escapement vector
        0,                          // orientation. angle between each character and hdc's x-axis
        font.weight,                // font weight
        font.italic,
        font.underline,
        false,                      // strke through
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,         // default precision
        CLIP_DEFAULT_PRECIS,        // default clip precision
        CLEARTYPE_QUALITY,          // quality
        DEFAULT_PITCH,              // pitch
        faceName.c_str());          // face name

    if (releaseDC)
    {
        ::ReleaseDC(NULL, hdc);
    }

    return newFont;
}