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

#include <pch.hpp>

#include <core/LibraryFactory.h>
#include <cube/TracklistModel.hpp>
#include <cube/TracklistColumn.hpp>

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/        TracklistModel::TracklistModel(musik::core::Query::ListBase *connectedQuery)
{
    this->SetRowCount(0);

    this->tracklist.OnTracks.connect(this,&TracklistModel::OnTracks);
    this->tracklist.OnTrackMeta.connect(this,&TracklistModel::OnTrackMeta);
    this->tracklist.ConnectToLibrary(musik::core::LibraryFactory::GetCurrentLibrary());

    if(connectedQuery){
        this->tracklist.ConnectToQuery(*connectedQuery);
    }
}

uistring            TracklistModel::CellValueToString(int rowIndex, ColumnRef column)
{

    TracklistColumn *tracklistColumn = (TracklistColumn*)column.get();


    typedef boost::basic_format<uichar> format;
//    return (format(_T("%1% %2%")) % column->Name() % (rowIndex + 1)).str();
    musik::core::TrackPtr track = this->tracklist[rowIndex];
    if(!track){
        return _T("");
    }else{
        const utfchar *value = track->GetValue(tracklistColumn->metaKey.c_str());
        if(value)
            return value;

        return _T("");
    }
}

void TracklistModel::OnTrackMeta(std::vector<int> &trackPositions){
    for(std::vector<int>::iterator row=trackPositions.begin();row!=trackPositions.end();++row){
        this->InvalidateData(*row);
    }
}
void TracklistModel::OnTracks(bool cleared){
    if(cleared){
        this->SetRowCount(0);
    }
    this->SetRowCount(this->tracklist.Size());
}


