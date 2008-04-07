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

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Window.hpp>
#include <win32cpp/Types.hpp>    // uichar, uistring

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

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
typedef sigslot::signal0<> TrackbarRepositionedEvent;

///\brief Trackbar allows the user to select a value from a range.
class Trackbar: public Window
{
private:    typedef Window base;

            ///\brief This event is emitted when the position of the slider is changed
public:     TrackbarRepositionedEvent   Repositioned;

public:     /*ctor*/            Trackbar(
                                    short minValue = 0, short maxValue = 100,
                                    TrackbarOrientation orientation = HorizontalTrack);

public:     void                SetRange(short minValue, short maxValue);
public:     int                 MinValue() const { return this->minValue; }
public:     int                 MaxValue() const { return this->maxValue; }
public:     void                SetTickFrequency(short tickFrequency = 0);
public:     short               TickFrequency() const { return this->tickFrequency; }
public:     void                SetTrackHeight(short trackHeight);
public:     short               TrackHeight() { return this->trackHeight; }
public:     void                SetThumbHeight(short thumbHeight);
public:     short               ThumbHeight() { return this->thumbHeight; }
public:     void                SetPosition(short position);
public:     short               Position() const { return this->position; }

        // private api
protected:  virtual HWND        Create(Window* parent);
protected:  virtual void        OnEraseBackground(HDC hdc);
protected:  virtual void        OnPaint();
protected:  virtual void        OnCreated();
protected:  virtual void        OnRepositioned();
protected:  virtual LRESULT     OnCustomDraw(NMCUSTOMDRAW& customDraw);
protected:  virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

protected:  short               minValue, maxValue, tickFrequency, position;
protected:  short               trackHeight, thumbHeight;
protected:  TrackbarOrientation   orientation;
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp