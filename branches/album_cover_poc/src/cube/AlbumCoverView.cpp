//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, mC2 Team
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
#include "pch.hpp"
#include "AlbumCoverView.hpp"
#include "AlbumCoverController.hpp"

//////////////////////////////////////////////////////////////////////////////
using namespace musik::cube;
//////////////////////////////////////////////////////////////////////////////

AlbumCoverView::AlbumCoverView(void)
{
}

AlbumCoverView::~AlbumCoverView(void)
{
}

void AlbumCoverView::OnCreated(){
}

void AlbumCoverView::OnPaint(){
    this->DrawAlbumcover(this->controller->thumbnail);
}


void AlbumCoverView::DrawAlbumcover(musik::core::Thumbnail &thumbnail){
    PAINTSTRUCT paintStruct;
    HDC hdc = ::BeginPaint(this->Handle(), &paintStruct);
    //
    {
        MemoryDC memDC(hdc, paintStruct.rcPaint);
        //
        ::SetBkColor(memDC, Color(255,255,255,0));

        LONG width   = paintStruct.rcPaint.right-paintStruct.rcPaint.left;
        LONG height  = paintStruct.rcPaint.bottom-paintStruct.rcPaint.top;
        if(width>0 && height>0){
            musik::core::Thumbnail::ImagePtr image  = thumbnail.GetImage(width,height);
            if(image){
                // Copy image to memDC
                HBITMAP imageBM( this->ToBitmap((*image)) );

                // Source
                HDC src_dc(::CreateCompatibleDC(memDC));
                ::SelectObject(src_dc,imageBM);


                ::BitBlt(memDC,0,0,width,height,src_dc,0,0,SRCCOPY);

                ::DeleteObject(imageBM);
            }
        }
    }
    //
    ::EndPaint(this->Handle(), &paintStruct);
}

HBITMAP AlbumCoverView::ToBitmap(const boost::gil::rgba8_image_t& image)
{
    HDC        dst_dc(::GetDC(NULL));
    HDC        src_dc(::CreateCompatibleDC(dst_dc));
    BITMAPINFO bitmap_info = {{ 0 }};
    VOID*      pv_bits;
    HBITMAP    bitmap_handle;
    LONG       image_width  = image.width();
    LONG       image_height = image.height();

    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = image_width;
    bitmap_info.bmiHeader.biHeight = -image_height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;         // four 8-bit components
    bitmap_info.bmiHeader.biCompression = BI_RGB;
    bitmap_info.bmiHeader.biSizeImage = image_width * image_height * 4;

    bitmap_handle = ::CreateDIBSection(src_dc, &bitmap_info, DIB_RGB_COLORS, &pv_bits, NULL, 0x0);

    if (bitmap_handle == 0)
        return bitmap_handle;

    boost::gil::bgra8_pixel_t*         dst_bits(static_cast<boost::gil::bgra8_pixel_t*>(pv_bits));
    boost::gil::rgba8_view_t::iterator first(boost::gil::view(const_cast<boost::gil::rgba8_image_t&>(image)).begin());
    boost::gil::rgba8_view_t::iterator last(boost::gil::view(const_cast<boost::gil::rgba8_image_t&>(image)).end());

    std::copy(first, last, dst_bits);

    return bitmap_handle;
}


