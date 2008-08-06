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

#include <win32cpp/ListView.hpp>
#include <cube/SourcesCategory.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

namespace musik { namespace cube {

//////////////////////////////////////////////////////////////////////////////
// SourcesListModel
//////////////////////////////////////////////////////////////////////////////

class InvalidSourcesCategoryException: public Exception { };

class SourcesListModel: public ListView::Model
{
        // nested classes
protected:  class CategoryCellRenderer;
protected:  class ItemCellRenderer;
protected:  class ItemRowRenderer;
protected:  friend class CategoryCellRenderer;
protected:  friend class ItemCellRenderer;
        // typedefs
public:     typedef ListView::Model base;
public:     typedef ListView::RowRendererRef RowRendererRef;
public:     typedef ListView::CellRendererRef CellRendererRef;
public:     typedef ListView::ColumnRef ColumnRef;
public:     typedef SourcesItemRef ItemRef;
public:     typedef SourcesCategory Category;
public:     typedef boost::shared_ptr<SourcesCategory> CategoryRef;
public:     typedef std::vector<CategoryRef> CategoryList;
private:    typedef std::map<int, CategoryRef> CategoryRefIndexMap;

        // signals
public:     sigslot::signal1<ItemRef> ActiveItemChanged;

        // public API
public:     /*ctor*/        SourcesListModel();
        //
public:     CategoryRef     AddCategory(CategoryRef category);
public:     CategoryRef     RemoveCategory(CategoryRef category);
public:     void            SelectedRowChanged(int newIndex);

        // private API
private:    int             VisibleItemCount();
private:    void            IndexCategories();
private:    ItemRef         ItemAtRowIndex(int rowIndex);

        // ListView::Model implementation
public:     virtual uistring CellValueToString(int rowIndex, ColumnRef column);
public:     virtual RowRendererRef RowRenderer(int rowIndex);
public:     virtual CellRendererRef CellRenderer(int rowIndex, ColumnRef column);

        // instance data
private:    CategoryList categories;
private:    CategoryRefIndexMap categoryIndexMap;
private:    ItemRef activeItem;
private:    int activeItemIndex;
};

//////////////////////////////////////////////////////////////////////////////
// SourcesListModel::CategoryCellRenderer
//////////////////////////////////////////////////////////////////////////////

class SourcesListModel::CategoryCellRenderer: 
    public ListView::TextCellRenderer<uistring>
{
public:     typedef ListView::TextCellRenderer<uistring> base;

public:     /*ctor*/ CategoryCellRenderer(const uistring& textValue);
public:     static void SetFonts(FontRef font);
protected:  virtual void Render(const ListView& listView, ListView::RenderParams& customDraw);

protected:  static FontRef sFont;
};

//////////////////////////////////////////////////////////////////////////////
// SourcesController::ItemCellRenderer
//////////////////////////////////////////////////////////////////////////////

class SourcesListModel::ItemCellRenderer: 
    public ListView::TextCellRenderer<uistring>
{
public:     typedef ListView::TextCellRenderer<uistring> base;

public:     /*ctor*/ ItemCellRenderer(SourcesListModel& owner, const uistring& textValue);
protected:  virtual void Render(const ListView& listView, ListView::RenderParams& customDraw);

private:    SourcesListModel& owner;
};

//////////////////////////////////////////////////////////////////////////////
// SourcesController::ItemRowRenderer
//////////////////////////////////////////////////////////////////////////////

class SourcesListModel::ItemRowRenderer: public ListView::RowRenderer
{
protected:  virtual void Render(const ListView& listView, ListView::RenderParams& customDraw)
            {
                // do nothing! let cell renderers do all the drawing!
            }
};

//////////////////////////////////////////////////////////////////////////////

} }     // musik::cube
