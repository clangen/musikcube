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

#ifndef _DEBUG
    // To be able to UPX the released executable
    extern "C" void tss_cleanup_implemented(void){}
#endif

//////////////////////////////////////////////////////////////////////////////
// dependencies
//////////////////////////////////////////////////////////////////////////////
#pragma warning (disable : 4996 4018 4482)

#include <core/config.h>
#include <core/LibraryFactory.h>
#include <core/ITrack.h>
#include <core/Track.h>
#include <core/TrackMeta.h>
#include <core/db/dbconfig.h>
#include <core/db/CachedStatement.h>
#include <core/db/Connection.h>
#include <core/db/ScopedTransaction.h>
#include <core/db/Statement.h>
#include <core/Library/Base.h>

#include <boost/shared_ptr.hpp>
#include <sigslot/sigslot.h>

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Types.hpp>
#include <win32cpp/WindowGeometry.hpp>
#include <win32cpp/Exception.hpp>
#include <win32cpp/Container.hpp>
#include <win32cpp/Font.hpp>
#include <win32cpp/Menu.hpp>
#include <win32cpp/Window.hpp>
#include <win32cpp/Application.hpp>
#include <win32cpp/TopLevelWindow.hpp>

//////////////////////////////////////////////////////////////////////////////

#include "vld/vld.h"

//////////////////////////////////////////////////////////////////////////////
