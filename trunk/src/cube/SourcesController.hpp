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

#include <cube/SourcesView.hpp>
#include <cube/SourcesModel.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace musik { namespace cube {

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////
// SourcesController
//////////////////////////////////////////////////////////////////////////////

class SourcesController: public EventHandler
{
private:    typedef ListView::ColumnRef ColumnRef;
private:    typedef SourcesModel::CategoryRef CategoryRef;
private:    typedef SourcesModel::ItemRef ItemRef;
private:    class ListController;

public:     /*ctor*/    SourcesController(SourcesView& sourcesView);

private:    void        OnViewCreated();
private:    void        OnActiveItemChanged(ItemRef newItem);
private:    void        OnModelCategoryAdded(CategoryRef category);
private:    void        OnModelCategoryRemoved(CategoryRef category);

protected:  SourcesView& view;
protected:  SourcesModel model;
protected:  boost::scoped_ptr<ListController> listController;
};

//////////////////////////////////////////////////////////////////////////////
// SourcesController::ListController
//////////////////////////////////////////////////////////////////////////////

class SourcesController::ListController: public EventHandler
{
private:    typedef boost::shared_ptr<SourcesListModel> ListModelRef;

public:     /*ctor*/        ListController(ListView& listView);
public:     ListModelRef    Model() { return this->sourcesListModel; }

private:    void            OnListCreated();
private:    void            OnListSelectionChanged();

private:    ListView& listView;
private:    ListModelRef sourcesListModel;
};

//////////////////////////////////////////////////////////////////////////////

} }     // musik::cube