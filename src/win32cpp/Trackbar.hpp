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

#include <win32cpp/Window.hpp>
#include <win32cpp/Types.hpp>    // uichar, uistring

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

class Trackbar; // forward declr

/*! */
enum TrackbarOrientation
{
    /*! */ VerticalTrack = TBS_VERT,
    /*! */ HorizontalTrack = TBS_HORZ
};

///\brief 
///Event used when position on trackbar is changed
///\see
///Trackbar
typedef sigslot::signal1<Trackbar*> TrackbarRepositionedEvent;

///\brief Trackbar allows the user to select a value from a range.
class Trackbar: public Window
{
private: // typedefs
    typedef Window base;

public: // events
    ///\brief This event is emitted when the position of the slider is changed
    TrackbarRepositionedEvent   Repositioned;

public: // constructors
public:     /*ctor*/            Trackbar(
                                    short minValue = 0, short maxValue = 100,
                                    TrackbarOrientation orientation = HorizontalTrack);

public: // methods
    void    SetRange(short minValue, short maxValue);
    int     MinValue() const { return this->minValue; }
    int     MaxValue() const { return this->maxValue; }
    void    SetTickFrequency(short tickFrequency = 0);
    short   TickFrequency() const { return this->tickFrequency; }
    void    SetTrackHeight(short trackHeight);
    short   TrackHeight() { return this->trackHeight; }
    void    SetThumbHeight(short thumbHeight);
    short   ThumbHeight() { return this->thumbHeight; }
    void    SetPosition(short position);
    short   Position() const { return this->position; }

protected: // methods
    virtual HWND        Create(Window* parent);
    virtual void        OnEraseBackground(HDC hdc);
    virtual void        OnPaint();
    virtual void        OnCreated();
    virtual void        OnRepositioned();
    virtual LRESULT     OnCustomDraw(NMCUSTOMDRAW& customDraw);
    virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

protected: // instance data
    short minValue, maxValue, tickFrequency, position;
    short trackHeight, thumbHeight;
    TrackbarOrientation   orientation;
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp