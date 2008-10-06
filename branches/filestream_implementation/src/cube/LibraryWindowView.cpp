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
#include <cube/LibraryWindowView.hpp>
#include <cube/LibraryWindowController.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    LibraryWindowView::LibraryWindowView()
{
}

/*dtor*/    LibraryWindowView::~LibraryWindowView()
{
    // make sure not to delete any SourcesItem views, and also
    // automatically delete the defaultView.
//    this->SetView(this->defaultView);
}
/*
void        SourcesView::OnCreated()
{
    // add main splitter as the top level window's main window
    this->splitter = this->AddChild(
        new Splitter(SplitColumn, this->listView, this->defaultView));

    this->splitter->SetAnchorSize(125);
 }

void        SourcesView::OnListViewCreated(Window* window)
{
    typedef ListView::Column Column;

    Size clientSize = this->listView->ClientSize();
 
    this->mainColumn = Column::Create(_T("Sources"), clientSize.width, TextAlignCenter);
    this->listView->AddColumn(this->mainColumn);

    this->listView->Resized.connect(this, &SourcesView::OnListViewResized);
    this->listView->ThemeChanged.connect(this, &SourcesView::OnListViewThemeChanged);
    this->listView->HotRowChanged.connect(this, &SourcesView::OnListViewHotRowChanged);

    this->listView->SetRowHeight(this->listView->RowHeight() + 2);
    this->listView->SetScrollBarVisibility(HorizontalScrollBar, false);
    this->listView->EnableColumnResizing(false);
    this->listView->EnableMultipleSelection(false);
    this->UpdateListViewBkgndColor();

    int itemHeight = this->listView->RowHeight();
    this->listView->SetRowHeight(max(itemHeight, 19));
}

void        SourcesView::SetView(Window* view)
{
    Window* oldView = 
        const_cast<Window*>(this->splitter->Child2());

    if (view && (view != oldView))
    {
        RedrawLock lockRedraw(this);

        oldView->SetVisible(false);
        this->splitter->SetChild2(view);
        view->SetVisible(true);
    }
}

void        SourcesView::OnListViewResized(Window* window, Size size)
{
    this->listView->SetColumnWidth(this->mainColumn, this->listView->ClientSize().width);
}

void        SourcesView::OnListViewThemeChanged(Window* window)
{
    this->UpdateListViewBkgndColor();
}

void        SourcesView::UpdateListViewBkgndColor()
{
    this->listView->SetBackgroundColor(Color::SystemColor(COLOR_BTNFACE));
}

void        SourcesView::OnListViewHotRowChanged(ListView* listView, int rowIndex)
{
    // Redraw the newly hot, and previously hot row to get the fancy
    // mouse hover effect. we need to redraw (and remember) the
    // previously hot row to make sure to "remove" the effect from
    // the old row.
    if (rowIndex >= 0) this->listView->RedrawRow(rowIndex);
    if (this->lastHotRowIndex != -1) this->listView->RedrawRow(this->lastHotRowIndex);
    this->lastHotRowIndex = rowIndex;
}
*/
