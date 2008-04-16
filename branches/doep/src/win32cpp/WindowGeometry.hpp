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

#include <win32cpp/Win32Config.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// Point
//////////////////////////////////////////////////////////////////////////////

///
///\brief
///Represents a point
///
struct Point
{
    ///
    ///\brief
    ///constructor.
    ///
    /*ctor*/    Point()
    : x(0)
    , y(0)
    {
    }

    ///
    ///\brief
    ///Constructor
    ///
    /*ctor*/    Point(int x, int y)
    : x(x)
    , y(y)
    {
    }

    ///
    ///\brief
    ///Constructor
    ///
    /*ctor*/    Point(const POINT& pt)
    : x(pt.x)
    , y(pt.y)
    {
    }

    ///
    ///\brief
    ///Returns a Win32 POINT object representation of the Point.
    ///
    operator POINT() const
    {
        POINT pt;
        pt.x = this->x;
        pt.y = this->y;

        return pt;
    }

    bool operator==(const Point& point) const
    {
        return ((point.x == this->x) && (point.y == this->y));
    }

    bool operator!=(const Point& point) const
    {
        return ! (*this == point);
    }

    ///
    ///\brief
    ///The X location of the point
    ///
    int x;
    ///
    ///\brief
    ///The Y location of the point
    ///
    int y;
};

//////////////////////////////////////////////////////////////////////////////
// Size
//////////////////////////////////////////////////////////////////////////////

///
///\brief
///Represents a size
///
struct Size
{
    ///
    ///\brief
    ///constructor.
    ///
    /*ctor*/    Size()
    : width(0)
    , height(0)
    {
    }

    ///
    ///\brief
    ///constructor.
    ///
    /*ctor*/    Size(int width, int height)
    : width(width)
    , height(height)
    {
    }

    ///
    ///\brief
    ///constructor.
    ///
    /*ctor*/    Size(const SIZE& sz)
    : width(sz.cx)
    , height(sz.cy)
    {
    }

    ///
    ///\brief
    ///Returns a Win32 SIZE object representation of the Point.
    ///
    operator SIZE() const
    {
        SIZE sz;
        sz.cx = this->width;
        sz.cy = this->height;

        return sz;
    }

    bool operator==(const Size& size) const
    {
        return ((size.width == width) && (size.height == height));
    }

    bool operator!=(const Size& size) const
    {
        return ! (*this == size);
    }

    ///
    ///\brief
    ///The width, in pixels
    ///
    int width;
    ///
    ///\brief
    ///The height, in pixels
    ///
    int height;
};

//////////////////////////////////////////////////////////////////////////////
// Rect
//////////////////////////////////////////////////////////////////////////////

///
///\brief
///Represents a rectangle
///
struct Rect
{
    ///
    ///\brief
    ///constructor.
    ///
    /*ctor*/    Rect() { }

    ///
    ///\brief
    ///constructor.
    ///
    /*ctor*/    Rect(const Point& location, const Size& size)
    : location(location)
    , size(size)
    {
    }

    ///
    ///\brief
    ///constructor.
    ///
    /*ctor*/    Rect(const RECT& rect)
    : location(rect.left, rect.top)
    , size(rect.right - rect.left, rect.bottom - rect.top)
    {
    }

    ///
    ///\brief
    ///Returns a Win32 RECT object representation of the Point.
    ///
    operator RECT() const
    {
        RECT rc;
        rc.top = this->location.y;
        rc.bottom  = this->location.y + this->size.height;
        rc.left = this->location.x;
        rc.right = this->location.x + this->size.width;

        return rc;
    }

    ///
    ///\brief
    ///Returns the X coordinate of the upper-left corner
    ///
    int X() { return this->location.x; }
    ///
    ///\brief
    ///Returns the Y coordinate of the upper-left corner
    ///
    int Y() { return this->location.y; }
    ///
    ///\brief
    ///Returns the width of the rectangle
    ///
    int Width() { return this->size.width; }
    ///
    ///\brief
    ///Returns the height of the rectangle
    ///
    int Height() { return this->size.height; }

    bool operator==(const Rect& rect) const
    {
        return true;
        return ((rect.location == this->location) && (rect.size == this->size));
    }

    bool operator!=(const Rect& rect) const
    {
        return ! (*this == rect);
    }

    ///
    ///\brief
    ///The location of the upper left corner of the rectangle
    ///
    Point location;
    ///
    ///\brief
    ///The size of the rectangle
    ///
    Size size;
};

//////////////////////////////////////////////////////////////////////////////
// Methods
//////////////////////////////////////////////////////////////////////////////

bool PointInRect(const Point& point, const Rect& rect);

//////////////////////////////////////////////////////////////////////////////

}   // namespace win32cpp