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
#include <win32cpp/ProgressBar.hpp>
#include <win32cpp/Application.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

///\brief
///Constructor.
///
///\param width
///The width of the edit box
///\param height
///The height of the edit box
/*ctor*/    ProgressBar::ProgressBar(int width, int height)
: base()
, width(width)
, height(height)
//, caption(caption)
{
    this->caption = _T("Progress Bar");
}

/*dtor*/    ProgressBar::~ProgressBar()
{
}

HWND        ProgressBar::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance();

    // create the window
    DWORD style = WS_CHILD | WS_VISIBLE;
    DWORD styleEx = this->styleEx;
    //
    HWND hwnd = CreateWindowEx(
        0,                      // ExStyle
        PROGRESS_CLASS,         // Class name
        this->caption.c_str(),  // Window name
        style,                  // Style
        0,                      // X
        0,                      // Y
        this->width,            // Width
        this->height,           // Height
        parent->Handle(),       // Parent
        NULL,                   // Menu
        hInstance,              // Instance
        NULL);                  // lParam

    DWORD error = GetLastError();
    if (hwnd)
    {
        ::SetWindowText(hwnd, this->caption.c_str());
    }

    return hwnd;
}

LRESULT     ProgressBar::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    return this->DefaultWindowProc(message, wParam, lParam);
}

///\brief
///Sets the progress bar to have a smooth bar
void        ProgressBar::SetMarqueeStyle()
{
    SetWindowLongPtr(this->Handle(), GWL_STYLE, GetWindowLongPtr(this->Handle(), GWL_STYLE) | PBS_MARQUEE);
    SetWindowPos(this->Handle(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
    return;
}

///\brief
///Sets the progress bar to have a smooth bar
void        ProgressBar::SetSmoothStyle()
{
    SetWindowLongPtr(this->Handle(), GWL_STYLE, GetWindowLongPtr(this->Handle(), GWL_STYLE) | PBS_SMOOTH);
    SetWindowPos(this->Handle(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
    return;
}

///\brief
///Sets the progress bar to have a smooth bar
void        ProgressBar::SetVerticalStyle()
{
    SetWindowLongPtr(this->Handle(), GWL_STYLE, GetWindowLongPtr(this->Handle(), GWL_STYLE) | PBS_VERTICAL);
    SetWindowPos(this->Handle(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
    return;
}

///\brief
///Sets the progress bar to have a marquee style (Windows XP or above)
///\param set
///Boolean to say whether to marquee or not
///\param delay
///Time in milliseconds between animation updates
void        ProgressBar::StartMarquee(bool set, unsigned int delay)
{
    this->SendMessage(PBM_SETMARQUEE, (WPARAM)set, (LPARAM)delay);
    return;
}

///\brief
///Sets the min and max values of the progress bar
///\param min
///The minimum value (unsigned)
///\param max
///The maximum value (unsigned)
void        ProgressBar::SetRange(unsigned int min, unsigned int max)
{
    this->SendMessage(PBM_SETRANGE, 0, MAKELPARAM(min,max));
}

///\brief
///Sets the current position of the progress bar
///\param pos
///The new position of the progress bar
void        ProgressBar::SetPos(int pos)
{
    this->SendMessage(PBM_SETPOS, pos, 0 /*must be 0. Not used*/);
    return;
}

///\brief
///Sets the current position of the progress bar
///\param pos
///The new step increment
void        ProgressBar::SetStepIncrement(int inc)
{
    this->SendMessage(PBM_SETSTEP, inc, 0 /*must be 0. Not used*/);
    return;
}

///\brief
///Steps the progress bar forward once. Default step is 10
void        ProgressBar::Step()
{
    this->SendMessage(PBM_STEPIT, 0, 0);
    return;
}