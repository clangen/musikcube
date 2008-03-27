//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2007, Casey Langen
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
#include <cube/TracklistController.hpp>
#include <cube/TracklistModel.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    TracklistController::TracklistController(TracklistView& view,musik::core::Query::ListBase *connectedQuery)
: view(view)
, model(new TracklistModel(connectedQuery))
{
    this->view.Handle()
        ? this->OnViewCreated()
        : this->view.Created.connect(this, &TracklistController::OnViewCreated);
}

void        TracklistController::OnViewCreated()
{
    typedef ListView::Column Column;

    ListView* listView = this->view.listView;
    listView->AddColumn(Column::Create(_T("track"), 50));
    listView->AddColumn(Column::Create(_T("title"), 200));
    listView->AddColumn(Column::Create(_T("visual_artist"), 100));
    listView->AddColumn(Column::Create(_T("album"), 100));
    listView->AddColumn(Column::Create(_T("visual_genre"), 75));

    int itemHeight = listView->RowHeight();
    listView->SetRowHeight(max(itemHeight, 17));
    //
    listView->EnableStripedBackground(true);
    listView->SetModel(this->model);
}

void        TracklistController::OnResized(Size size)
{
}