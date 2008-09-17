//////////////////////////////////////////////////////////////////////////////
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
#include <cube/SourcesController.hpp>
#include <cube/SourcesListModel.hpp>
#include <cube/SourcesView.hpp>
#include <cube/dialog/NewPlaylistController.hpp>

#include <core/MessageQueue.h>
#include <core/Query/Playlists.h>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////
// SourcesController
//////////////////////////////////////////////////////////////////////////////

/*ctor*/    SourcesController::SourcesController(SourcesView& view,musik::core::LibraryPtr library)
: view(view)
, model(SourcesModel(library))
, listController(new ListController(*view.listView))
, library(library)
, LibraryCallbackTimer(20)
{
    this->view.Handle()
        ? this->OnViewCreated(&view)
        : this->view.Created.connect(this, &SourcesController::OnViewCreated);

    this->listController->Model()->ActiveItemChanged.connect(
        this, &SourcesController::OnActiveItemChanged);

	// Connect timer to librarys callbacks
	this->LibraryCallbackTimer.ConnectToWindow(&view);
    this->LibraryCallbackTimer.OnTimeout.connect(this,&SourcesController::QueryQueueLoop);
    library->OnQueryQueueStart.connect(this,&SourcesController::QueryQueueStart);
    library->OnQueryQueueEnd.connect(this,&SourcesController::QueryQueueEnd);

	musik::core::MessageQueue::MessageEvent("NewPlaylist").connect(this,&SourcesController::OnNewPlaylist);

}

void        SourcesController::OnViewCreated(Window* window)
{
    this->model.CategoryAdded.connect(this, &SourcesController::OnModelCategoryAdded);
    this->model.CategoryRemoved.connect(this, &SourcesController::OnModelCategoryRemoved);
    this->model.Load();

	this->UpdatePlaylists();

}

void        SourcesController::OnModelCategoryAdded(CategoryRef category)
{
    this->listController->Model()->AddCategory(category);

    // TEMP HACK
    if (category->Caption() == _(_T("View")))
    {
        this->listController->Model()->SelectedRowChanged(1);
    }
}

void        SourcesController::OnModelCategoryRemoved(CategoryRef category)
{
    this->listController->Model()->RemoveCategory(category);
}

void        SourcesController::OnActiveItemChanged(ItemRef item)
{
    if (item && item->View())
    {
        this->view.SetView(item->View());
    }
    else
    {
        this->view.SetView(this->view.defaultView);
    }
}

void SourcesController::QueryQueueStart(){
	this->LibraryCallbackTimer.ConnectToWindow(&this->view);
    this->LibraryCallbackTimer.Start();
}

void SourcesController::QueryQueueEnd(){
    this->LibraryCallbackTimer.Stop();
}

void SourcesController::QueryQueueLoop(){
	this->library->RunCallbacks();
}

void SourcesController::OnNewPlaylist(void* data){
	if(data==this->library.get()){
		win32cpp::TopLevelWindow popupDialog(_(_T("New Playlist")));
		popupDialog.SetMinimumSize(Size(300, 150));

		dialog::NewPlaylistController newPlaylist(popupDialog,this->library);

		popupDialog.ShowModal(TopLevelWindow::FindFromAncestor(&this->view));

		this->UpdatePlaylists();
	}
}

void SourcesController::UpdatePlaylists(){
	musik::core::Query::Playlists playlistQuery;
	playlistQuery.PlaylistList.connect(this,&SourcesController::OnPlaylists);
	this->library->AddQuery(playlistQuery);
}

void SourcesController::OnPlaylists(std::vector<musik::core::tracklist::Ptr> trackLists){
	this->model.OnPlaylists(trackLists);

	this->listController->sourcesListModel->Update();
//	this->view.Redraw();
}

//////////////////////////////////////////////////////////////////////////////
// SourcesController::ListController
//////////////////////////////////////////////////////////////////////////////

#define SC SourcesController

/*ctor*/    SC::ListController::ListController(ListView& listView)
: listView(listView)
, sourcesListModel(new SourcesListModel())
{
    this->listView.Handle()
        ? this->OnListCreated(&listView)
        : this->listView.Created.connect(this, &SC::ListController::OnListCreated);

    this->listView.SelectionChanged.connect(this, &SC::ListController::OnListSelectionChanged);
}

void        SC::ListController::OnListCreated(Window* window)
{
    this->listView.SetModel(this->sourcesListModel);
}

void        SC::ListController::OnListSelectionChanged(ListView* listView)
{
    this->sourcesListModel->SelectedRowChanged(this->listView.SelectedRow());
}
