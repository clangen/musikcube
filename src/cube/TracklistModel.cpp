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

#include "pch.hpp"
#include <cube/TracklistModel.hpp>
#include <cube/TracklistColumn.hpp>

#include <win32cpp/Utility.hpp>
#include <win32cpp/ApplicationThread.hpp>

#include <core/LibraryFactory.h>
#include <core/PlaybackQueue.h>
#include <core/MetaKey.h>
#include <core/tracklist/IRandomAccess.h>
#include <core/tracklist/Standard.h>


using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/        TracklistModel::TracklistModel(musik::core::Query::ListBase *connectedQuery,musik::core::tracklist::Ptr setTracklist)
: currentPosition(-1)
{

	this->tracklist = setTracklist;

    this->SetRowCount(0);

    this->tracklist->TracksUpdated.connect(this,&TracklistModel::OnTracks);
    this->tracklist->TrackMetaUpdated.connect(this,&TracklistModel::OnTrackMeta);
    this->tracklist->PositionChanged.connect(this,&TracklistModel::OnPositionChanged);

    this->ConnectToQuery(connectedQuery);
}

uistring            TracklistModel::CellValueToString(int rowIndex, ColumnRef column)
{

    TracklistColumn *tracklistColumn = (TracklistColumn*)column.get();


    typedef boost::basic_format<uichar> format;
    musik::core::TrackPtr track = this->tracklist->TrackWithMetadata(rowIndex);
    if(track){
		const utfchar *value = track->GetValue(tracklistColumn->metaKey.c_str());
        if(value){
            switch(tracklistColumn->metaKeyType){
                case musik::core::MetaKey::Duration:
                    UINT64 duration = boost::lexical_cast<int>(value);
                    UINT64 days(duration/86400);
                    duration    = duration%86400;
                    UINT64 hours(duration/3600);
                    duration    = duration%3600;
                    UINT64 minutes(duration/60);
                    duration    = duration%60;
                    utfstring result;
                    if (minutes < 10)
                        result += _T("0");
                    result += boost::lexical_cast<utfstring>(minutes) + _T(":");
                    if (duration < 10)
                        result += _T("0");
                    result += boost::lexical_cast<utfstring>(duration);
                    return win32cpp::Escape(result);
                    break;
            }
            if(rowIndex==this->currentPosition){
    			return win32cpp::Escape(value)+_T("***");
            }
			return win32cpp::Escape(value);
		}
    }

    return _T("");
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
    this->SetRowCount(this->tracklist->Size());
}


void TracklistModel::OnRowActivated(int row){
    this->tracklist->SetCurrentPosition(row);
    musik::core::PlaybackQueue::Instance().Play(this->tracklist);
}

void TracklistModel::OnPlayNow(win32cpp::ListView::RowIndexList& selectedRows){
    // Create a temporary tracklist to put into the "now playing" tracklist
    musik::core::tracklist::Ptr selectedTracklist(new musik::core::tracklist::Standard());
    selectedTracklist->SetLibrary(this->tracklist->Library());

    for(win32cpp::ListView::RowIndexList::iterator row=selectedRows.begin();row!=selectedRows.end();++row){
        musik::core::TrackPtr track( (*this->tracklist)[*row] );
        if(track){
            selectedTracklist->AppendTrack(track);
        }
    }
    musik::core::PlaybackQueue::Instance().Play(selectedTracklist);
}

void TracklistModel::OnEnqueue(win32cpp::ListView::RowIndexList& selectedRows){
    // Create a temporary tracklist to put into the "now playing" tracklist
    musik::core::tracklist::Ptr selectedTracklist(new musik::core::tracklist::Standard());
    selectedTracklist->SetLibrary(this->tracklist->Library());

    for(win32cpp::ListView::RowIndexList::iterator row=selectedRows.begin();row!=selectedRows.end();++row){
        musik::core::TrackPtr track( (*this->tracklist)[*row] );
        if(track){
            selectedTracklist->AppendTrack(track);
        }
    }
    musik::core::PlaybackQueue::Instance().Append(selectedTracklist);
}

void TracklistModel::ConnectToQuery(musik::core::Query::ListBase *connectedQuery){
    if(connectedQuery){
        this->tracklist->ConnectToQuery(*connectedQuery);
    }
}

void TracklistModel::OnPositionChanged(int activeRow,int oldActiveRow){
    if(!win32cpp::ApplicationThread::InMainThread()){
        win32cpp::ApplicationThread::Call2(this,&TracklistModel::OnPositionChanged,activeRow,oldActiveRow);
        return;
    }

    if(activeRow!=-1){
        this->InvalidateData(activeRow);
    }
    if(oldActiveRow!=-1){
        this->InvalidateData(oldActiveRow);
    }
    this->currentPosition   = activeRow;
}
