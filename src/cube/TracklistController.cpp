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

#include "pch.hpp"
#include <cube/TracklistController.hpp>
#include <cube/TracklistView.hpp>
#include <cube/TracklistModel.hpp>
#include <cube/TracklistColumn.hpp>
#include <cube/TracklistInfoView.hpp>
#include <core/LibraryFactory.h>
#include <win32cpp/Label.hpp>
#include <boost/format.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    TracklistController::TracklistController(
    TracklistView& view,
    musik::core::Query::ListBase *connectedQuery,
    musik::core::tracklist::Ptr tracklist)
: view(view)
, model(new TracklistModel(connectedQuery, tracklist))
{
    this->view.Handle()
        ? this->OnViewCreated(&view)
        : this->view.Created.connect(this, &TracklistController::OnViewCreated);


    // Connect the tracklists TracklistInfoUpdated
    TracklistModel* model   = (TracklistModel*)this->model.get();
    if(model){
        model->tracklist->TracklistInfoUpdated.connect(this,&TracklistController::OnTracklistInfo);
    }

}

void        TracklistController::OnViewCreated(Window* window)
{
    typedef ListView::Column Column;

    ListView* listView = this->view.listView;
    this->AddColumn(_(_T("Track")),"track", 50);
    this->AddColumn(_(_T("Title")),"title", 200);
    this->AddColumn(_(_T("Artist")),"visual_artist", 100);
    this->AddColumn(_(_T("Album")),"album", 100);
    this->AddColumn(_(_T("Genre")),"visual_genre", 75);
    this->AddColumn(_(_T("Duration")),"duration", 50);
    this->AddColumn(_(_T("BPM")),"bpm", 75);

    int itemHeight = listView->RowHeight();
    listView->SetRowHeight(max(itemHeight, 17));
    //
    listView->EnableStripedBackground(true);
    listView->SetModel(this->model);
    listView->RowActivated.connect(this, &TracklistController::OnRowActivated);
    listView->Resized.connect( this, &TracklistController::OnResized);
    listView->ColumnClicked.connect( this, &TracklistController::OnColumnSort );

    TracklistModel* model   = (TracklistModel*)this->model.get();
    if(model){
        model->ConnectToQuery(&this->sortQuery);
    }


    // Add the context menu
    this->contextMenu   = win32cpp::Menu::CreatePopup();
    // Play now menu
    win32cpp::MenuItemRef playNowMenu   = this->contextMenu->Items().Append(win32cpp::MenuItem::Create(_T("Play now")));
    playNowMenu->Activated.connect(this,&TracklistController::OnPlayNow);
    // Enqueue menu
    win32cpp::MenuItemRef enqueueMenu   = this->contextMenu->Items().Append(win32cpp::MenuItem::Create(_T("Enqueue")));
    enqueueMenu->Activated.connect(this,&TracklistController::OnEnqueue);
    //
    listView->SetContextMenu(contextMenu);


}

void        TracklistController::OnRowActivated(ListView* listView, int row)
{
    ((TracklistModel*)this->model.get())->OnRowActivated(row);
}


void        TracklistController::AddColumn(const utfchar *name,const char *metakey,int size){
    this->view.listView->AddColumn(ListView::ColumnRef(new TracklistColumn(metakey,name, size)));

    ((TracklistModel*)this->model.get())->tracklist->AddRequestedMetakey(metakey);
}

void TracklistController::OnTracklistInfo(UINT64 tracks,UINT64 duration,UINT64 filesize){
    if(this->view.infoView){
        typedef boost::basic_format<uichar> format;

        this->view.infoView->trackCountLabel->SetCaption( (format(_T("%1%"))%tracks).str() );

        UINT64 days(duration/86400);
        duration    = duration%86400;
        UINT64 hours(duration/3600);
        duration    = duration%3600;
        UINT64 minutes(duration/60);
        duration    = duration%60;
        this->view.infoView->durationLabel->SetCaption( (format(_T("%d days, %02d:%02d:%02d"))%days%hours%minutes%duration).str() );

        if(filesize<1048576){
            this->view.infoView->sizeLabel->SetCaption( (format(_T("%.2f KB"))%((double)filesize/(double)1024)).str() );
        }else if(filesize<1073741824){
            this->view.infoView->sizeLabel->SetCaption( (format(_T("%.2f MB"))%((double)filesize/(double)1048576)).str() );
        }else{
            this->view.infoView->sizeLabel->SetCaption( (format(_T("%.2f GB"))%((double)filesize/(double)1073741824)).str() );
        }
    }
}

void TracklistController::OnResized(Window* window, Size size){
    int rows    = size.height/this->view.listView->RowHeight();
    TracklistModel* model   = (TracklistModel*)this->model.get();
    if(model && rows>10 && rows<100){
        model->tracklist->HintNumberOfRows(rows);
    }

}

void TracklistController::OnColumnSort(ListView *listView,ColumnRef column){
    TracklistColumn *tracklistColumn = (TracklistColumn*)column.get();
    TracklistModel* model   = (TracklistModel*)this->model.get();
    if(tracklistColumn && model){
        // what to sort by
        std::list<std::string> sortList;
        sortList.push_back(tracklistColumn->metaKey);
        this->sortQuery.SortByMetaKeys(sortList);

        // Add the tracks to sort
        this->sortQuery.AddTracks(*(model->tracklist));

		musik::core::LibraryPtr library( model->tracklist->Library());
		if(library){
	        library->AddQuery(this->sortQuery,musik::core::Query::CancelSimilar);
		}

        this->sortQuery.ClearTracks();
    }
}

void TracklistController::OnPlayNow(win32cpp::MenuItemRef menu){
    ListView::RowIndexList selectedRows(this->view.listView->SelectedRows());

    TracklistModel* model   = (TracklistModel*)this->model.get();
    if(model){
        model->OnPlayNow(selectedRows);
    }    
}

void TracklistController::OnEnqueue(win32cpp::MenuItemRef menu){
    ListView::RowIndexList selectedRows(this->view.listView->SelectedRows());

    TracklistModel* model   = (TracklistModel*)this->model.get();
    if(model){
        model->OnEnqueue(selectedRows);
    }    
}
