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
#include <cube/MetadataFilterController.hpp>
#include <cube/MetadataFilterModel.hpp>
#include <cube/BrowseController.hpp>
#include <core/Common.h>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////
// MetadataFilterController
//////////////////////////////////////////////////////////////////////////////

/*ctor*/    MetadataFilterController::MetadataFilterController(ListView& listView, const uistring& metadataKey,BrowseController *browseController)
: listView(listView)
, metadataKey(metadataKey)
, parent(browseController)
{
    this->metadataKeyA    = musik::core::ConvertUTF8(this->metadataKey);
    this->model.reset(new MetadataFilterModel(this));
    this->listView.Handle()
        ? this->OnViewCreated()
        : this->listView.Created.connect(this, &MetadataFilterController::OnViewCreated);

    this->listView.SelectionChanged.connect(this,&MetadataFilterController::OnSelectionChanged);
}

void        MetadataFilterController::OnSelectionChanged(){
    win32cpp::ListView::RowIndexList selectedRows(this->listView.SelectedRows());

    musik::core::MetadataValueVector &metadata = ((MetadataFilterModel*)this->model.get())->metadata;

    this->parent->selectionQuery.ClearMetadata(this->metadataKeyA.c_str());

    bool firstRowSelected(false);
    for(win32cpp::ListView::RowIndexList::iterator row=selectedRows.begin();row!=selectedRows.end() && !firstRowSelected;++row){
        if((*row)==0){
            firstRowSelected    = true;
        }else{
            this->parent->selectionQuery.SelectMetadata(this->metadataKeyA.c_str(),metadata[(*row)-1]->id);
        }
    }

    // Check if first row is selected, then clear the list
    if(firstRowSelected){
        this->parent->selectionQuery.ClearMetadata(this->metadataKeyA.c_str());
    }

    this->parent->SendQuery();
}

void        MetadataFilterController::OnViewCreated()
{
    this->listView.SetScrollBarVisibility(HorizontalScrollBar, false);

    typedef ListView::Column Column;

    Size clientSize = this->listView.ClientSize();

    this->mainColumn = Column::Create(this->metadataKey.c_str(), clientSize.width);
    //
    this->listView.AddColumn(this->mainColumn);

    this->listView.EnableColumnResizing(false);
    this->listView.SetModel(this->model);
    //
    int itemHeight = this->listView.RowHeight();
    this->listView.SetRowHeight(max(itemHeight, 17));

    this->listView.Resized.connect(
        this, &MetadataFilterController::OnResized);
}

void        MetadataFilterController:: OnResized(Size size)
{
    this->listView.SetColumnWidth(this->mainColumn, this->listView.ClientSize().width);
}
