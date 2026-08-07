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
 * @file Font.hpp
 * @brief Lightweight wrapper around a Win32 HFONT.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Font encapsulates
 * an HFONT along with its attributes (face, size, weight, style) and can
 * draw text directly to a device context. Font objects are shared through
 * FontRef smart pointers.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Types.hpp>
#include <win32cpp/Exception.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// Font
//////////////////////////////////////////////////////////////////////////////

struct Font;

/** @brief Shared pointer to a Font object. */
typedef std::shared_ptr<Font> FontRef;

/** @brief Specifies how text is aligned when rendered.
 *  @details Values map directly to the Win32 DrawText DT_* flags. */
enum TextAlignment
{
    TextAlignLeft = DT_LEFT,        /*!< left-aligned text */
    TextAlignCenter = DT_CENTER,    /*!< horizontally centered text */
    TextAlignRight = DT_RIGHT       /*!< right-aligned text */
};

/** @brief Wraps a Win32 HFONT and its display attributes.
 *  @details Exposes the face, point size, weight, italic and underline
 *           attributes of the underlying font, and can render text into
 *           a device context. Instances are created via the static
 *           Create() factories and owned through FontRef.
 *  @see FontRef */
struct Font
{
public: // types
    /** @brief Thrown when an invalid font weight is specified. */
    class InvalidFontWeightException: public Exception { };

private: // constructors
    /** @brief Constructs a default font (system UI font). */
    /*ctor*/    Font();

    /** @brief Constructs a font from named attributes.
     *  @param face the font face name
     *  @param pointSize the requested point size (-1 for the default)
     *  @param bold whether the font should be bold
     *  @param italic whether the font should be italic
     *  @param underline whether the font should be underlined */
    /*ctor*/    Font(const uistring& face, unsigned pointSize = -1,
                bool bold = false, bool italic = false, bool underline = false);

    /** @brief Constructs a font from a Win32 LOGFONT.
     *  @param logFont the logical font to copy
     *  @param hdc device context used to compute the font height */
    /*ctor*/    Font(const LOGFONT& logFont, HDC hdc = NULL);


public: // creation methods
    /** @brief Creates a default font.
     *  @return a shared pointer to the new font */
    static FontRef Create()
    {
        return FontRef(new Font());
    }

    /** @brief Creates a font from named attributes.
     *  @param face the font face name
     *  @param pointSize the requested point size (-1 for the default)
     *  @param bold whether the font should be bold
     *  @param italic whether the font should be italic
     *  @param underline whether the font should be underlined
     *  @return a shared pointer to the new font */
    static FontRef Create(const uistring& face, int pointSize = -1,
    bool bold = false, bool italic = false, bool underline = false)
    {
        return FontRef(new Font(face, pointSize, bold, italic, underline));
    }

    /** @brief Creates a font from a Win32 LOGFONT.
     *  @param logFont the logical font to copy
     *  @param hdc device context used to compute the font height
     *  @return a shared pointer to the new font */
    static FontRef Create(const LOGFONT& logFont, HDC hdc = NULL)
    {
        return FontRef(new Font(logFont, hdc));
    }

public: // destructor
    /** @brief Destroys the font, releasing the HFONT. */
    /*dtor*/    ~Font();

public: // methods
    /** @brief Draws the given text into the device context.
     *  @param hdc the target device context
     *  @param rect the bounding rectangle for the text
     *  @param caption the text to draw
     *  @param alignment how the text should be aligned */
    void        DrawToHDC(HDC hdc, const Rect& rect, const uistring& caption, TextAlignment = TextAlignLeft);
    /** @brief Returns the font face name.
     *  @return the face name */
    uistring    FaceName() const;
    /** @brief Sets the font face name.
     *  @param faceName the new face name */
    void        SetFaceName(const uistring& faceName);
    /** @brief Returns the point size of the font.
     *  @return the point size */
    unsigned    PointSize() const;
    /** @brief Sets the point size of the font.
     *  @param pointSize the new point size */
    void        SetPointSize(unsigned pointSize);
    /** @brief Returns whether the font is bold.
     *  @return true if bold */
    bool        Bold() const;
    /** @brief Sets whether the font is bold.
     *  @param bold true to make the font bold */
    void        SetBold(bool bold = true);
    /** @brief Returns the font weight.
     *  @return the Win32 font weight value */
    int         Weight() const;
    /** @brief Sets the font weight.
     *  @param weight the Win32 font weight value (e.g. FW_BOLD) */
    void        SetWeight(int weight);
    /** @brief Returns whether the font is italic.
     *  @return true if italic */
    bool        Italic() const;
    /** @brief Sets whether the font is italic.
     *  @param italic true to make the font italic */
    void        SetItalic(bool italic = true);
    /** @brief Returns whether the font is underlined.
     *  @return true if underlined */
    bool        Underline() const;
    /** @brief Sets whether the font is underlined.
     *  @param underline true to underline the font */
    void        SetUnderline(bool underline = true);
    /** @brief Returns the underlying HFONT, creating it if needed.
     *  @return the HFONT handle */
    HFONT       GetHFONT();

    /** @brief Creates a raw HFONT from the given Font.
     *  @param font the font attributes to use
     *  @param hdc device context used to compute the font height
     *  @return the created HFONT */
    static HFONT  CreateHFONT(const Font& font, HDC hdc);

protected: // methods
    /** @brief Marks the font as needing re-creation. */
    void    Invalidate() { this->invalid = true; }
    /** @brief Creates the HFONT from the current attributes.
     *  @param hdc device context used for scaling
     *  @return true on success */
    bool    InitializeFont(HDC hdc);
    /** @brief Creates the HFONT from a Win32 LOGFONT.
     *  @param font the logical font to use
     *  @param hdc device context used for scaling
     *  @return true on success */
    bool    InitializeFont(const LOGFONT& font, HDC hdc);

private: // instance data
    uistring faceName;  /**< face name of the font */
    unsigned pointSize; /**< point size of the font */
    bool italic, underline; /**< style flags */
    int weight;         /**< Win32 font weight */
    bool invalid;       /**< true when the HFONT must be re-created */
    HFONT font;         /**< underlying Win32 font handle */
};

//////////////////////////////////////////////////////////////////////////////

} // win32cpp
