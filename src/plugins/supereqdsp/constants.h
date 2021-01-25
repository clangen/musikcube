//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
//
// All rights reserved.
//
// Note: this musikcube plugin (supereq) is not distributed under a BSD
// license like most other project components because it consumes GPL2
// code.
//
//////////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef WIN32
    #ifndef WINVER
        #define WINVER 0x0601
    #endif

    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0601
    #endif

    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define DLLEXPORT __declspec(dllexport)

    #include <windows.h>
#else
    #define DLLEXPORT
#endif