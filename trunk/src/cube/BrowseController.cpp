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
#include <cube/BrowseController.hpp>
#include <cube/BrowseView.hpp>
#include <cube/TracklistController.hpp>
#include <core/LibraryFactory.h>
#include <core/tracklist/LibraryList.h>
#include <core/Indexer.h>

#include <win32cpp/ApplicationThread.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    BrowseController::BrowseController(BrowseView& view,musik::core::LibraryPtr library)
: view(view)
, tracklistController(NULL)
, library(library)
{
    view.Handle()
        ? this->OnViewCreated(&view)
        : this->view.Created.connect(this, &BrowseController::OnViewCreated);
}

/*dtor*/    BrowseController::~BrowseController()
{
    delete this->tracklistController;
}

void        BrowseController::OnViewCreated(Window* window)
{

	// Create a tracklist, connected to current library
	musik::core::tracklist::Ptr browseTrackList(new musik::core::tracklist::LibraryList(this->library));

    this->tracklistController = new TracklistController(*this->view.tracklistView,&this->selectionQuery,browseTrackList);

    // create all the metadata filter controllers
    typedef BrowseView::FilterViewList FilterViewList;
    typedef BrowseView::FilterToMetadataKeyMap FilterToMetadataKeyMap;

    FilterViewList& filterViews = this->view.filterViews;
    FilterToMetadataKeyMap& filterKeyMap = this->view.filterKeyMap;

    FilterViewList::iterator it = filterViews.begin();
    while (it != filterViews.end())
    {
        uistring metadataKey = filterKeyMap[*it];
        this->filterControllers.push_back(
            FilterControllerRef(new FilterController(**it, metadataKey,this)));

        ++it;
    }

    // Connect the OnLibraryUpdate to the indexer
    if(this->library->Indexer()){
        this->library->Indexer()->TrackRefreshed.connect(this,&BrowseController::OnLibraryUpdate);
    }

    // Startup by sending an empty query
    this->SendQuery();

}

void BrowseController::SendQuery(){
	this->library->AddQuery(this->selectionQuery,musik::core::Query::CancelSimilar);
}

void BrowseController::OnLibraryUpdate(){
    if(!win32cpp::ApplicationThread::InMainThread()){
        win32cpp::ApplicationThread::Call0(this,&BrowseController::OnLibraryUpdate);
        return;
    }
    this->SendQuery();

}
