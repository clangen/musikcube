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

#include <pch.hpp>
#include <cube/BrowseController.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    BrowseController::BrowseController(BrowseView& view)
: view(view)
, tracklistController(NULL)
{
    view.Handle()
        ? this->OnViewCreated()
        : this->view.Created.connect(this, &BrowseController::OnViewCreated);
}

/*dtor*/    BrowseController::~BrowseController()
{
    delete this->tracklistController;
}

void        BrowseController::OnViewCreated()
{
    this->tracklistController = new TracklistController(*this->view.tracklistView);

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
            FilterControllerRef(new FilterController(**it, metadataKey)));

        ++it;
    }
}
