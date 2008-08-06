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
// Forward declare
namespace musik { namespace cube {
    class TracklistView;
} }
//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Types.hpp>
#include <win32cpp/ListView.hpp>
#include <core/config.h>
#include <core/Query/ListBase.h>
#include <core/tracklist/Standard.h>
#include <core/Query/SortTracks.h>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

namespace musik { namespace cube {

//////////////////////////////////////////////////////////////////////////////
// TracklistController
//////////////////////////////////////////////////////////////////////////////

class TracklistController : public EventHandler
{
private:    
    typedef ListView::ColumnRef ColumnRef;
    typedef std::vector<ColumnRef> ColumnList;
    typedef ListView::ModelRef ModelRef;

public:     
    /*ctor*/    TracklistController(
                    TracklistView& listView,
                    musik::core::Query::ListBase *connectedQuery = NULL,
                    musik::core::tracklist::Standard::Ptr tracklist = musik::core::tracklist::Standard::Ptr());

private:  
    void        OnViewCreated(Window* window);
    void        OnResized(Window* window,Size size);
    void        OnRowActivated(ListView* listView, int row);
    void        AddColumn(const utfchar *name, const char *metakey, int size);
    void        OnTracklistInfo(UINT64 tracks,UINT64 duration,UINT64 filesize);
    void        OnColumnSort(ListView *listView,ColumnRef column);

    ModelRef model;
    TracklistView& view;
    ColumnList columns;
    musik::core::Query::SortTracks sortQuery;

};

//////////////////////////////////////////////////////////////////////////////

} } // musik::cube
