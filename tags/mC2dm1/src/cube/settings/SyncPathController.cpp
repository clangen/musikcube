//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, mC2 Team
//
// Sources and Binaries of: mC2
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
#include <cube/settings/SyncPathController.hpp>
#include <cube/settings/SyncPathModel.hpp>
#include <cube/SettingsController.hpp>
#include <core/Indexer.h>
#include <core/LibraryFactory.h>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube::settings;

//////////////////////////////////////////////////////////////////////////////

SyncPathController::SyncPathController(ListView &listView,musik::cube::SettingsController *settingsController)
: listView(listView)
, settingsController(settingsController)
{
    this->model.reset(new SyncPathModel(this));
    this->listView.Handle()
        ? this->OnViewCreated(&listView)
        : this->listView.Created.connect(this, &SyncPathController::OnViewCreated);
}


void SyncPathController::OnViewCreated(Window* window){
    this->listView.SetScrollBarVisibility(HorizontalScrollBar, false);

    typedef ListView::Column Column;

    Size clientSize = this->listView.ClientSize();

    this->mainColumn = Column::Create(_T("Path"), clientSize.width);
    this->listView.AddColumn(this->mainColumn);

    this->listView.EnableColumnResizing(false);
    this->listView.SetModel(this->model);
    //
    int itemHeight = this->listView.RowHeight();
    this->listView.SetRowHeight(max(itemHeight, 17));

    this->listView.Resized.connect(
        this, &SyncPathController::OnResized);

}

void SyncPathController::OnResized(Window* window, Size size)
{
    this->listView.SetColumnWidth(this->mainColumn, this->listView.ClientSize().width);
}

void SyncPathController::RemoveSelectedPaths(){

    SyncPathModel* model    = (SyncPathModel*)this->model.get();
    musik::core::Indexer *indexer   = musik::core::LibraryFactory::GetCurrentLibrary()->Indexer();
    if(indexer && model){

        win32cpp::ListView::RowIndexList selectedRows(this->listView.SelectedRows());
        for(win32cpp::ListView::RowIndexList::iterator row=selectedRows.begin();row!=selectedRows.end();++row){
            indexer->RemovePath(model->paths[*row]);
        }

    }
    
}
