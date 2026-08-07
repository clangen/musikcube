//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2007, Casey Langen
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

/**
 * @file Trackbar.hpp
 * @brief Slider control for selecting a value from a range.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Trackbar wraps the
 * Win32 trackbar control (msctls_trackbar32), allowing the user to drag a
 * thumb to choose a value between a minimum and maximum. Emits the
 * TrackbarRepositionedEvent when the position changes.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Window.hpp>
#include <win32cpp/Types.hpp>    // uichar, uistring

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

class Trackbar; // forward declr

/** @brief The orientation of a Trackbar. */
enum TrackbarOrientation
{
    /*! */ VerticalTrack = TBS_VERT,   /**< vertical slider */
    /*! */ HorizontalTrack = TBS_HORZ  /**< horizontal slider */
};

/** @brief Signal emitted when the position on the trackbar changes.
 *  @see Trackbar */
typedef sigslot::signal1<Trackbar*> TrackbarRepositionedEvent;

/** @brief Trackbar allows the user to select a value from a range.
 *  @details Wraps the Win32 trackbar control. The position can be read and
 *           written, the value range can be configured, and the Repositioned
 *           signal is emitted whenever the user moves the thumb. */
class Trackbar: public Window
{
private: // typedefs
    typedef Window base;

public: // events
    /** @brief Emitted when the position of the slider is changed. */
    TrackbarRepositionedEvent   Repositioned;

public: // constructors
    /** @brief Constructs a trackbar with the given range.
     *  @param minValue the minimum value
     *  @param maxValue the maximum value
     *  @param orientation the slider orientation */
public:     /*ctor*/            Trackbar(
                                    short minValue = 0, short maxValue = 100,
                                    TrackbarOrientation orientation = HorizontalTrack);

public: // methods
    /** @brief Sets the value range.
     *  @param minValue the minimum value
     *  @param maxValue the maximum value */
    void    SetRange(short minValue, short maxValue);
    /** @brief Returns the size of the value range.
     *  @return maxValue - minValue */
    short   Range() { return this->maxValue - this->minValue; }
    /** @brief Returns the minimum value.
     *  @return the minimum */
    int     MinValue() const { return this->minValue; }
    /** @brief Returns the maximum value.
     *  @return the maximum */
    int     MaxValue() const { return this->maxValue; }
    /** @brief Sets how often tick marks are drawn.
     *  @param tickFrequency the tick frequency (0 to disable) */
    void    SetTickFrequency(short tickFrequency = 0);
    /** @brief Returns the tick frequency.
     *  @return the tick frequency */
    short   TickFrequency() const { return this->tickFrequency; }
    /** @brief Sets the track (line) height.
     *  @param trackHeight the height in pixels */
    void    SetTrackHeight(short trackHeight);
    /** @brief Returns the track height.
     *  @return the height in pixels */
    short   TrackHeight() { return this->trackHeight; }
    /** @brief Sets the thumb (handle) height.
     *  @param thumbHeight the height in pixels */
    void    SetThumbHeight(short thumbHeight);
    /** @brief Returns the thumb height.
     *  @return the height in pixels */
    short   ThumbHeight() { return this->thumbHeight; }
    /** @brief Sets the current position.
     *  @param position the new position value */
    void    SetPosition(short position);
    /** @brief Returns the current position.
     *  @return the position value */
    short   Position() const { return this->position; }

protected: // methods
    /** @brief Creates the underlying HWND. */
    virtual HWND        Create(Window* parent);
    /** @brief Clears the background before painting. */
    virtual void        OnEraseBackground(HDC hdc);
    /** @brief Paints the trackbar. */
    virtual void        OnPaint();
    /** @brief Handles post-creation setup. */
    virtual void        OnCreated();
    /** @brief Emits the Repositioned signal. */
    virtual void        OnRepositioned();
    /** @brief Handles NM_CUSTOMDRAW notifications. */
    virtual LRESULT     OnCustomDraw(NMCUSTOMDRAW& customDraw);
    /** @brief Processes window messages. */
    virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

protected: // instance data
    short minValue, maxValue, tickFrequency, position; /**< range and position values */
    short trackHeight, thumbHeight;                    /**< visual dimensions */
    TrackbarOrientation   orientation;                 /**< slider orientation */
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
