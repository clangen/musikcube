//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2008, Andr� W�sten
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

/**
 * @file ImageList.hpp
 * @brief Wrapper around a Win32 image list (HIMAGELIST).
 *
 * Part of the win32cpp native Win32 GUI wrapper library. ImageList wraps a
 * Win32 image list and provides a small set of operations to add, remove,
 * clear and query images at a fixed size and color depth.
 */

#pragma once

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Types.hpp>
#include <win32cpp/Exception.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// ImageList
//////////////////////////////////////////////////////////////////////////////

/** @brief A collection of same-sized images managed by Win32.
 *  @details Wraps the Win32 common controls image list (HIMAGELIST). All
 *           images share a fixed width, height and color depth. Used, for
 *           example, to supply icons to ComboBox and ListView models. */
class ImageList {
private:
    HIMAGELIST imagelistHandle; /**< the underlying HIMAGELIST */
    int imageWidth, imageHeight; /**< dimensions of each image */
    int imageDepth;              /**< color depth of each image */

    ImageList()
    {
        ;
    }
protected:
public:
    /** @brief Color depth used for the images in the list. */
    enum ColorDepth {
        Color4  = ILC_COLOR4,  /*!< 4 bits per pixel */
        Color8  = ILC_COLOR8,  /*!< 8 bits per pixel */
        Color16 = ILC_COLOR16, /*!< 16 bits per pixel */
        Color32 = ILC_COLOR32  /*!< 32 bits per pixel */
    };

    /** @brief Creates an empty image list with the given settings.
     *  @param width the width of each image, in pixels
     *  @param height the height of each image, in pixels
     *  @param depth the color depth of each image */
    ImageList(int width, int height, ImageList::ColorDepth depth);
    /** @brief Destroys the image list. */
    ~ImageList();

    /** @brief Returns the underlying HIMAGELIST.
     *  @return the image list handle */
    HIMAGELIST  Handle() const;
    /** @brief Returns the number of images in the list.
     *  @return the image count */
    int         Count() const;

    /** @brief Adds a bitmap to the list.
     *  @param image the HBITMAP to add
     *  @return the index of the added image */
    int         Add(HBITMAP image);
    /** @brief Loads and adds an image from a file.
     *  @param filename path of the image file
     *  @param transparence true to treat the image as transparent
     *  @return the index of the added image */
    int         Add(const uistring& filename, bool transparence = false);

    /** @brief Removes the image at the given index.
     *  @param index the index of the image to remove
     *  @return true if the image was removed */
    bool        Remove(int index);
    /** @brief Removes all images from the list.
     *  @return true if the list was cleared */
    bool        Clear();

    /** @brief Retrieves information about the image at the given index.
     *  @param index the image index
     *  @param info the IMAGEINFO structure to fill
     *  @return true if the information was retrieved */
    bool        Info(int index, IMAGEINFO *info);
};

//////////////////////////////////////////////////////////////////////////////

}
