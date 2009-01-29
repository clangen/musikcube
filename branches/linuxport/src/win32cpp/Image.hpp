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

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Types.hpp>
#include <win32cpp/Exception.hpp>

#include <boost/mpl/vector.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/gil/extension/dynamic_image/dynamic_image_all.hpp>
#include <boost/gil/gil_all.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

class Image;

///
///\brief A shared pointer to a Font object
///
typedef boost::shared_ptr<Image> ImageRef;

///\brief
///Image provides a light-weight wrapper around an image. It is backed
///by boost::gil.
///
///\see
///ImageRef
class Image
{
public: // types
    class InvalidFontWeightException: public Exception { };

private: // types
    typedef boost::mpl::vector<
        boost::gil::gray8_image_t,
        boost::gil::gray16_image_t,
        boost::gil::rgb8_image_t,
        boost::gil::rgb16_image_t> SupportedFormats;

    typedef boost::gil::any_image<
        SupportedFormats> ImageType;

private: // constructors
    /*ctor*/    Image();

public: // creation methods
    static ImageRef Create(const uistring& filename);

public: // destructor
    /*dtor*/    ~Image();

public: // methods
    void        DrawToHDC(HDC hdc, const Point& point);

private: // instance data
    ImageType image;
};

//////////////////////////////////////////////////////////////////////////////

} // win32cpp
