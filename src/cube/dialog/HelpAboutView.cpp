//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, mC2 Team
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

#include "pch.hpp"
#include <cube/dialog/HelpAboutView.hpp>
#include <core/LibraryFactory.h>
#include <win32cpp/Label.hpp>
#include <win32cpp/Button.hpp>
#include <win32cpp/LinearLayout.hpp>
#include <win32cpp/BarLayout.hpp>
#include <win32cpp/EditView.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube::dialog;
using namespace win32cpp;

volatile bool anim_running = false;
const int anim_w = 400;
const int anim_h = 300;

#define anim_color(R, G, B) \
    ((unsigned short) ( \
        ( ( (R) >> 3 ) << 11) | \
        ( ( (G) >> 2 ) << 5 ) | \
        ( ( (B) >> 3 )      ) \
    ))
    

//////////////////////////////////////////////////////////////////////////////

HelpAboutView::HelpAboutView()
: Frame(NULL, FramePadding(6))
, drawingThread(NULL)
{
}

void HelpAboutView::DrawingThread(HWND hwnd, BITMAPINFO* bmi)
{
    // Variable decl
    unsigned short* screen;
    unsigned char   sintable[1024];
    unsigned short  plasmapal[256];
    int             plasmawave[4] = {0, 0, 0, 0};

    // Create surface
    screen = new unsigned short[anim_w * anim_h];

    // Calc sin table for plasma effect
    for(int i=0; i<1024; i++) {
        sintable[i] = 128 + 127.0 * (sin((double)i * 3.1415 / 512.0));
    }

    // Calc color table for plasma effect
    for(int i=0; i<256; i++) {
        int 
            r = 20 + 19.0 * sin((double)i * 3.1415 /  40.0),
            g = 40 + 39.0 * sin((double)i * 3.1415 /  40.0),
            b = 120 + 79.0 *  cos((double)i * 3.1415 / 120.0);

        plasmapal[i] = anim_color(r, g, b);
    }

    // Start time of animation
    unsigned long time_start = ::GetTickCount();

    // Start drawing loop
    while(anim_running) {
        // Calc current time
        float time = (float)(::GetTickCount() - time_start) / 1000.0f;

        // Plasma
        {
            // Plasma is done by addition over 4 parametric, circular plasma waves
            int sx[4], sy[4], wx[4], wy[4];

            sx[0] =  600 * sin(time * 2.0  + 2.0);
            sy[0] =  600 * cos(time * 1.9  - 1.3);
            sx[1] =  140 * sin(-time * 1.8 + 2.4);
            sy[1] =  140 * cos(-time * 1.7 + 1.2);
            sx[2] =  560 * sin(time * 1.6  - 0.4);
            sy[2] =  460 * cos(time * 1.5  + 2.5);
            sx[3] =  340 * sin(-time * 1.4 - 1.7);
            sy[3] =  340 * cos(-time * 1.3 + 1.8);

            for(int i=0; i<4; i++) {
                plasmawave[i] += sx[i];
                wy[i] = plasmawave[i];
            }

            // Process scanline-based
            unsigned short* sb = screen;
            for(int y=0; y<anim_h; y++) {
                for(int i=0; i<4; i++) {
                    wx[i] = wy[i];
                }

                for(int x=0; x<anim_w; x++) {
                    *sb = plasmapal[(
                        sintable[(wx[0] >> 8) & 0x3ff] +
                        sintable[(wx[1] >> 8) & 0x3ff] +
                        sintable[(wx[2] >> 8) & 0x3ff] +
                        sintable[(wx[3] >> 8) & 0x3ff]
                    ) >> 2];

                    ++sb;

                    for(int i=0; i<4; i++) {
                        wx[i] += sx[i];
                    }
                }

                for(int i=0; i<4; i++) {
                    wy[i] += sy[i];
                }
            }
        }

        // Blit surface
        HDC hdc = ::GetDC(hwnd);
        ::SetDIBitsToDevice(hdc, 0, 0, anim_w, anim_h, 0, 0, 0, anim_h, (void*)screen, bmi, DIB_RGB_COLORS);

        // Some idle time to give other threads the chance to process
        ::Sleep(1);
    }

    delete[] screen; screen = NULL;
}

void HelpAboutView::StartDrawingThread()
{
    anim_running = true;

    this->drawingThread = new boost::thread(
        &HelpAboutView::DrawingThread,  
        this->Parent()->Handle(),
        this->bitmapinfo
    ); 
}

void HelpAboutView::OnCreated()
{
    // Initialize bitmap info header for 16-bit GDI graphics
    this->bitmapinfo = (BITMAPINFO*) malloc(sizeof(BITMAPINFOHEADER) + 12);
    ::ZeroMemory(&this->bitmapinfo->bmiHeader, sizeof(BITMAPINFOHEADER));

    bitmapinfo->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bitmapinfo->bmiHeader.biWidth       = anim_w;
    bitmapinfo->bmiHeader.biHeight      = -anim_h;
    bitmapinfo->bmiHeader.biBitCount    = 16;
    bitmapinfo->bmiHeader.biPlanes      = 1;
    bitmapinfo->bmiHeader.biCompression = BI_BITFIELDS;
    
    // Color masks. Since we got 16-bit, we have 5-6-5 RGB, so we need to configure
    // the bitmap info header according to this color space.
    *((long*)bitmapinfo->bmiColors +0) = 0xF800;
    *((long*)bitmapinfo->bmiColors +1) = 0x07E0;
    *((long*)bitmapinfo->bmiColors +2) = 0x001F;	
}

void HelpAboutView::OnDestroyed() 
{
    // Indicate drawing thread to stop
    anim_running = false;

    // Wait for thread exit
    this->drawingThread->join();

    // Delete thread and other stuff
    if(this->drawingThread) {
        delete this->drawingThread;
    }

    if(this->bitmapinfo) {
        free(this->bitmapinfo);
        this->bitmapinfo = NULL;
    }

}