//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Casey Langen, André Wösten
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
#include <win32cpp/ImageList.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

ImageList::ImageList(int width, int height, ImageList::ColorDepth depth) :
    imageWidth(width),
    imageHeight(height),
    imageDepth(depth),
    imagelistHandle(NULL)
{
    ::InitCommonControls(); 

    this->imagelistHandle = ::ImageList_Create(imageWidth, imageHeight, depth, 1, 128);
    if(!this->imagelistHandle) {
        throw Exception("Cannot create image list!");
    }
}

ImageList::~ImageList()
{
}

HIMAGELIST ImageList::Handle() const
{
    return this->imagelistHandle;
}

int ImageList::Count() const
{
    return ::ImageList_GetImageCount(this->imagelistHandle);
}

int ImageList::Add(HBITMAP image)
{
    return ::ImageList_Add(this->imagelistHandle, image, image);
}

int ImageList::Add(const uistring& filename, bool transparence)
{
    UINT fuLoad = LR_LOADFROMFILE;

    if(transparence) {
        fuLoad |= LR_LOADTRANSPARENT;
    }

    HANDLE image = ::LoadImage(
        NULL,
        filename.c_str(),
        IMAGE_BITMAP,
        this->imageWidth,
        this->imageHeight,
        fuLoad
    );

    if(image) {
        return this->Add((HBITMAP)image);
    }

    return -1;
}

bool ImageList::Remove(int index) 
{
    return !!(::ImageList_Remove(this->imagelistHandle, index));
}

bool ImageList::Clear() 
{
    return !!(::ImageList_Remove(this->imagelistHandle, -1));
}

bool ImageList::Info(int index, IMAGEINFO *info)
{
    return !!(::ImageList_GetImageInfo(this->imagelistHandle, index, info));
}