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

#include <pch.hpp>
#include <win32cpp/Image.hpp>

#include <boost/filesystem.hpp>
#include <boost/gil/gil_all.hpp>
#include <boost/gil/extension/io/dynamic_io.hpp>
#include <boost/gil/extension/io/png_dynamic_io.hpp>

#include <utf8/utf8.h>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

///\brief
///Constructor.
///
///\param filename
///A filename of an image to load
/*ctor*/    Image::Image()
{

}

/*dtor*/    Image::~Image()
{

}

///\brief
///Creates a new Image
///
///\param filename
///Path to image to be loaded
///
///\returns
///The image at filename, NULL if the image couldn't be loaded.
ImageRef    Image::Create(const uistring& filename) 
{
    ImageRef result = ImageRef(new Image());

    try
    {
        // convert the UTF16 path to a UTF8 path
        std::string utf8filename;
        utf8::utf16to8(filename.begin(), filename.end(), std::back_inserter(utf8filename));
        //
        if (utf8filename.size())
        {
            // make sure the file exists
            boost::filesystem::path path(utf8filename.c_str());
            if (boost::filesystem::exists(path))
            {
                std::string extension = boost::filesystem::extension(path);
                std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

                // figure out what type of image to read based on the extension
                if (extension == ".png")
                {
                    boost::gil::png_read_image(utf8filename.c_str(), result->image);
                }
            }
        }
    }
    catch (std::ios_base::failure)
    {
        // TODO: log
        return ImageRef();
    }

    return result;
}

///\brief
///Draws this bitmap to the specified device context.
///
///\param hdc
///The destination device context
///\param point
///Where to draw the image on the HDC
void        Image::DrawToHDC(HDC hdc, const Point& point)
{
}