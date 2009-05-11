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
    class SourcesView;
} }
//////////////////////////////////////////////////////////////////////////////

#include <cube/SourcesModel.hpp>
#include <core/Library/Base.h>
#include <win32cpp/Timer.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace musik { namespace cube {

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////
// SourcesController
//////////////////////////////////////////////////////////////////////////////

class SourcesController: public EventHandler
{
private:    
	typedef ListView::ColumnRef ColumnRef;
	typedef SourcesModel::CategoryRef CategoryRef;
	typedef SourcesModel::ItemRef ItemRef;

	class ListController;

public:     
	/*ctor*/    SourcesController(SourcesView& sourcesView,musik::core::LibraryPtr library);
	musik::core::LibraryPtr library;

private:    
	void OnViewCreated(Window* window);
	void OnActiveItemChanged(ItemRef newItem);
	void OnModelCategoryAdded(CategoryRef category);
	void OnModelCategoryRemoved(CategoryRef category);
	void OnNewPlaylist(void* data);

protected:  
	SourcesView& view;
	SourcesModel model;
	boost::scoped_ptr<ListController> listController;

	Timer LibraryCallbackTimer;
	void QueryQueueStart();
	void QueryQueueEnd();
	void QueryQueueLoop();

//	void UpdatePlaylists();
//	void OnPlaylists(std::vector<musik::core::tracklist::Ptr> trackLists);
};

//////////////////////////////////////////////////////////////////////////////
// SourcesController::ListController
//////////////////////////////////////////////////////////////////////////////

class SourcesController::ListController: public EventHandler
{
private:    typedef boost::shared_ptr<SourcesListModel> ListModelRef;

public:     /*ctor*/        ListController(ListView& listView);
public:     ListModelRef    Model() { return this->sourcesListModel; }

private:    void            OnListCreated(Window* window);
private:    void            OnListSelectionChanged(ListView* listView);

private:    ListView& listView;
private:    
	friend class SourcesController;
	ListModelRef sourcesListModel;
};

//////////////////////////////////////////////////////////////////////////////

} }     // musik::cube
