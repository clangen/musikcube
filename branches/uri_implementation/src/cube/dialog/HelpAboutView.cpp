//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright  2008, mC2 Team
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
#include <core/Common.h>
#include <cube/dialog/HelpAboutView.hpp>
#include <core/LibraryFactory.h>
#include <win32cpp/Label.hpp>
#include <win32cpp/Button.hpp>
#include <win32cpp/LinearLayout.hpp>
#include <win32cpp/BarLayout.hpp>
#include <win32cpp/EditView.hpp>

// GDI+ for loading the image
#include <atlbase.h>
#include <gdiplus.h>

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

#define extract_r(C) ( (   (C) >> 11  & 0x1F ) << 3 )
#define extract_g(C) ( ( ( (C) >> 5 ) & 0x3F ) << 2 )
#define extract_b(C) ( (   (C)        & 0x1F ) << 3 )

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
        sintable[i] = 128 + (unsigned char)(127.0 * (sin((double)i * 3.1415 / 512.0)));
    }

    // Calc color table for plasma effect
    for(int i=0; i<256; i++) {
        int
            r =  20 + (int)(19.0 * sin((double)i * 3.1415 /  40.0)),
            g =  40 + (int)(39.0 * sin((double)i * 3.1415 /  40.0)),
            b = 120 + (int)(79.0 * cos((double)i * 3.1415 / 120.0));

        plasmapal[i] = anim_color(r, g, b);
    }

    // Start time of animation
    unsigned long time_start = ::GetTickCount();

    // Init GDI*
    Gdiplus::GdiplusStartupInput gp_si;
    ULONG_PTR gp_tok;
    Gdiplus::GdiplusStartup(&gp_tok, &gp_si, NULL);
    
    // Load logo
    Gdiplus::Bitmap* gp_logo = Gdiplus::Bitmap::FromFile(
        (musik::core::GetApplicationDirectory() + _T("resources\\logo.png")).c_str()
    );

    // Convert logo to correct format
    unsigned int 
        logo_width = gp_logo->GetWidth(),
        logo_height = gp_logo->GetHeight();
    unsigned int
        *logo = new unsigned int[logo_width * logo_height], 
        *plogo = logo;        
    BYTE
        *logo_alpha = new BYTE[logo_width * logo_height],
        *plogo_alpha = logo_alpha;

    for(int y=0; y<logo_height; y++) {
        for(int x=0; x<logo_width; x++) {
            Gdiplus::Color col;
            gp_logo->GetPixel(x, y, &col);

            *plogo = anim_color(col.GetRed(), col.GetGreen(), col.GetBlue());
            ++plogo;

            *plogo_alpha = col.GetAlpha();
            ++plogo_alpha;
        }
    }

    // Create font for text scroller
    HFONT font = ::CreateFont(
        14, 
        0, 
        0, 
        0,
        FW_NORMAL, 
        0, 
        0, 
        0, 
        ANSI_CHARSET, 
        OUT_DEFAULT_PRECIS, 
        CLIP_DEFAULT_PRECIS, 
        ANTIALIASED_QUALITY | CLEARTYPE_QUALITY, 
        DEFAULT_PITCH, 
        _T("Courier New")
    );
    
    // Define Texts here, there are played in this order
    uistring texts[] = {
        _T("--+ musikCube 2 | milestone 2 +--"),
        _T("created by"),
        _T("avatar"),
        _T("doep"),
        _T("bjorn"),
        _T("jooles"),
        _T("naaina"),
        _T("mC2 are copyright (c) mC2 Team 2007-2008"),
        _T("win32cpp are copyright (c) Casey Langen 2007-2008"),
        _T("mC2 wouldn't be possible without these projects:"),
        _T("tuniac (http://tuniac.sf.net)"),
        _T("boost (http://www.boost.org)"),
        _T("sqlite3 (http://www.sqlite.org)"),
        _T("taglib (http://developer.kde.org/~wheeler/taglib)")
    };
    
    RECT cr;
    ::GetClientRect(hwnd, &cr);

    RECT textrect;
    textrect.left = 0;
    textrect.top = 200;
    textrect.right = cr.right;
    textrect.bottom = 220;

    // Start drawing loop
    while(anim_running) {
        // Calc current time
        double time = (double)(::GetTickCount() - time_start) / 1000.0;

        // Plasma
        {
            // Plasma is done by addition over 4 parametric, circular plasma waves
            int sx[4], sy[4], wx[4], wy[4];

            sx[0] =  (int)(600.0 * sin(time * 2.0  + 2.0));
            sy[0] =  (int)(600.0 * cos(time * 1.9  - 1.3));
            sx[1] =  (int)(140.0 * sin(-time * 1.8 + 2.4));
            sy[1] =  (int)(140.0 * cos(-time * 1.7 + 1.2));
            sx[2] =  (int)(560.0 * sin(time * 1.6  - 0.4));
            sy[2] =  (int)(460.0 * cos(time * 1.5  + 2.5));
            sx[3] =  (int)(340.0 * sin(-time * 1.4 - 1.7));
            sy[3] =  (int)(340.0 * cos(-time * 1.3 + 1.8));

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

            // Display logo
            double alpha = 0.7 + 0.3 * sin(0.3 * time * 3.1415);
            int lx = 127, ly = 40;
            for(int y=0; y<logo_height; y++) {
                // blackit is true in order to create the black noisy stripes
                bool blackit = false;
                if(
                    (
                        (alpha >= 0.4 && alpha <= 0.42) || 
                        (alpha >= 0.71 && alpha <= 0.714)
                    ) && !(y % (1 + (rand() % logo_height)))) {
                    blackit = true;
                }

                for(int x=0; x<logo_width; x++) {
                    unsigned short* cp = &screen[(lx + x) + ((ly + y) * anim_w)];

                    unsigned short 
                        c = logo[x + y * logo_width],
                        s = *cp;

                    BYTE a = logo_alpha[x + y * logo_width];
                    
                    if(blackit) c = 0, a = 0;

                    double ac = alpha * ((double)a / 255.0f);

                    unsigned int
                        r = (unsigned int)((extract_r(c) * ac) + (extract_r(s) * (1.0 - ac))),
                        g = (unsigned int)((extract_g(c) * ac) + (extract_g(s) * (1.0 - ac))),
                        b = (unsigned int)((extract_b(c) * ac) + (extract_b(s) * (1.0 - ac)));

                    *cp = anim_color(r, g, b);
                }
            }
        }

        // Blit surface
        HDC hdc = ::GetDC(hwnd);
        ::SetDIBitsToDevice(hdc, 0, 0, anim_w, anim_h, 0, 0, 0, anim_h, (void*)screen, bmi, DIB_RGB_COLORS);

        // Draw text
        {
            static unsigned int textidx = 0;

            ::SetBkMode(hdc, TRANSPARENT);
            ::SetTextColor(hdc, 0xDDDDAA);
            ::SelectObject(hdc, font);

            static double tdlast = 0.0f;

            double td = time - tdlast;
            if(td > 2.5) { 

                if(textidx+1 == sizeof(texts) / sizeof(uistring)) {
                    textidx = 0;
                } else {
                    textidx++;
                }

                tdlast = time;
            }

            ::DrawText(
                hdc, 
                texts[textidx].c_str(), 
                (int)texts[textidx].size() + 1, 
                &textrect,
                DT_CENTER
            );
        }

        // Some idle time to give other threads the chance to process
        Sleep(5);
    }

    // Delete GDI Objects
    DeleteObject(font);

    // Delete resources
    delete gp_logo; gp_logo = NULL;
    delete logo; logo = NULL;
    delete logo_alpha; logo_alpha = NULL;
    delete[] screen; screen = NULL;
    
    // Shutdown GDI+
    Gdiplus::GdiplusShutdown(gp_tok);
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
