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
#include <win32cpp/Trackbar.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    Trackbar::Trackbar(short minValue, short maxValue, TrackbarOrientation orientation)
: base()
, minValue(minValue)
, maxValue(maxValue)
, orientation(orientation)
, trackHeight(2)
, thumbHeight(16)
, tickFrequency(0)
{
}

HWND        Trackbar::Create(Window* parent)
{
    DWORD style = WS_CHILD | WS_VISIBLE | TBS_NOTICKS | TBS_BOTH | TBS_FIXEDLENGTH | this->orientation;
    //
    return CreateWindowEx(
        NULL,                       // ExStyle
        TRACKBAR_CLASS,             // Class name
        _T(""),                     // Window name
        style,                      // Style
        0,                          // X
        0,                          // Y
        120,                        // Width
        36,                         // Height
        parent->Handle(),           // Parent
        NULL,                       // Menu
        Application::Instance(),    // Instance
        NULL);                      // lParam
}

LRESULT     Trackbar::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{   
    switch (message)
    {
    case WM_NOTIFY:
        {
            if ( ! lParam)
            {
                break;
            }

            NMHDR* notifyHeader = reinterpret_cast<NMHDR*>(lParam);
            switch (notifyHeader->code)
            {
            case NM_CUSTOMDRAW:
                {
                    // The header ctrl also emits NM_CUSTOMDRAW messages
                    if (notifyHeader->hwndFrom == this->Handle())
                    {
                        NMCUSTOMDRAW* customDraw = reinterpret_cast<NMCUSTOMDRAW*>(notifyHeader);
                        return this->OnCustomDraw(*customDraw);
                    }
                }
                break;
            }
        }
        break;
    }

    return base::WindowProc(message, wParam, lParam);
}

void        Trackbar::OnCreated()
{
    LONG range = MAKELONG(this->minValue, this->maxValue);
    ::SendMessage(this->Handle(), TBM_SETRANGE, FALSE, range);

    this->SetThumbHeight(this->thumbHeight);
}

LRESULT     Trackbar::OnCustomDraw(NMCUSTOMDRAW& customDraw)
{
    switch (customDraw.dwDrawStage)
    {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTERASE;
    }

    switch (customDraw.dwItemSpec)
    {
    case TBCD_CHANNEL:  // seems to be the most reliable place to draw the background
        {
            Rect clientRect = this->ClientRect();
            if (::GetBkMode(customDraw.hdc) != TRANSPARENT)
            {
                HBRUSH brush = ::CreateSolidBrush(Color::SystemColor(COLOR_BTNFACE));
                ::FillRect(customDraw.hdc, &(RECT)clientRect, brush);
                ::DeleteObject(brush);
            }
            
            // windows doesn't automatically center the track vertically if no ticks
            // are visible on the trackbar.
            if (this->tickFrequency == 0)
            {
                Rect channelRect = customDraw.rc;
                //             
                channelRect.location.y = (clientRect.Height() - channelRect.Height()) / 2;
                customDraw.rc = channelRect;
            }

            short topOffset = this->trackHeight / 2;
            customDraw.rc.top -= (topOffset);
            customDraw.rc.bottom += (this->trackHeight - topOffset);
        }
        break;
    case TBCD_THUMB:
        {
            // center the thumb vertically if no ticks are visible
            if (this->tickFrequency == 0)
            {
                Rect clientRect = this->ClientRect();
                Rect channelRect = customDraw.rc;

                channelRect.location.y = (clientRect.Height() - channelRect.Height()) / 2;
                customDraw.rc = channelRect;
            }
        }

        break;
    case TBCD_TICS:
        break;
    }

    return CDRF_DODEFAULT;
}

void        Trackbar::OnEraseBackground(HDC hdc)
{
}

void        Trackbar::OnPaint()
{
    ::InvalidateRect(this->Handle(), NULL, FALSE);
    base::OnPaint();
}

void        Trackbar::SetTickFrequency(short tickFrequency)
{
    DWORD style = ::GetWindowLong(this->Handle(), GWL_STYLE);
    //
    if ( ! tickFrequency)
    {
        style |= TBS_NOTICKS;
        style &= ~TBS_AUTOTICKS;
    }
    else
    {
        style &= ~TBS_NOTICKS;
        style |= TBS_AUTOTICKS;
    }
    //
    ::SetWindowLong(this->Handle(), GWL_STYLE, style);

    this->tickFrequency = tickFrequency;
    this->SendMessage(TBM_SETTICFREQ, (WPARAM) this->tickFrequency, NULL);
}

void        Trackbar::SetThumbHeight(short thumbHeight)
{
    this->thumbHeight = thumbHeight;
    ::SendMessage(this->Handle(), TBM_SETTHUMBLENGTH, this->thumbHeight, 0);
}

void        Trackbar::SetTrackHeight(short trackHeight)
{
    this->trackHeight = trackHeight;
    this->Redraw();
}
