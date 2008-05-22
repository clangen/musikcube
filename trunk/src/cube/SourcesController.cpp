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
#include <cube/SourcesListModel.hpp>
#include <cube/SourcesController.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////
// SourcesController
//////////////////////////////////////////////////////////////////////////////

/*ctor*/    SourcesController::SourcesController(SourcesView& view)
: view(view)
, model(SourcesModel())
, listController(new ListController(*view.listView))
{
    this->view.Handle()
        ? this->OnViewCreated(&view)
        : this->view.Created.connect(this, &SourcesController::OnViewCreated);

    this->listController->Model()->ActiveItemChanged.connect(
        this, &SourcesController::OnActiveItemChanged);
}

void        SourcesController::OnViewCreated(Window* window)
{
    this->model.CategoryAdded.connect(this, &SourcesController::OnModelCategoryAdded);
    this->model.CategoryRemoved.connect(this, &SourcesController::OnModelCategoryRemoved);
    this->model.Load();
}

void        SourcesController::OnModelCategoryAdded(CategoryRef category)
{
    this->listController->Model()->AddCategory(category);

    // TEMP HACK
    if (category->Caption() == _T("View"))
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