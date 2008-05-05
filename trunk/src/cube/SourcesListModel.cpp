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

#include <pch.hpp>
#include <cube/SourcesListModel.hpp>

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////
// SourcesListModel
//////////////////////////////////////////////////////////////////////////////

typedef ListView::RowRendererRef RowRendererRef;
typedef ListView::CellRendererRef CellRendererRef;
typedef ListView::ColumnRef ColumnRef;
typedef SourcesListModel::CategoryRef CategoryRef;
typedef SourcesListModel::ItemRef ItemRef;

/*ctor*/        SourcesListModel::SourcesListModel()
: activeItemIndex(-1)
{
}

uistring            SourcesListModel::CellValueToString(int rowIndex, ColumnRef column)
{
 typedef boost::basic_format<uichar> format;
 return (format(_T("%1% %2%")) % column->Name() % (rowIndex + 1)).str();
}

RowRendererRef      SourcesListModel::RowRenderer(int rowIndex)
{
 return RowRendererRef(new ItemRowRenderer());
}

CellRendererRef     SourcesListModel::CellRenderer(int rowIndex, ColumnRef column)
{
    uistring value = this->CellValueToString(rowIndex, column);

    // figure out if the value at rowIndex is a category
    CategoryRefIndexMap::iterator categoryIter = this->categoryIndexMap.find(rowIndex);
    if (categoryIter != this->categoryIndexMap.end())
    {
        Category& category = *((categoryIter->second).get());
        return CellRendererRef(new CategoryCellRenderer(category.Caption()));
    }

    // not a category, so find the category it belongs to.
    ItemRef item = this->ItemAtRowIndex(rowIndex);
    return CellRendererRef(new ItemCellRenderer(*this, item->Caption()));
}

ItemRef             SourcesListModel::ItemAtRowIndex(int rowIndex)
{
    CategoryRefIndexMap::iterator categoryIter = this->categoryIndexMap.begin();
    CategoryRefIndexMap::iterator end = this->categoryIndexMap.end();

    while ((categoryIter != end) && (categoryIter->first < rowIndex))
    {
        CategoryRefIndexMap::iterator nextCategory = categoryIter;
        nextCategory++;

        if ((nextCategory == end) || (nextCategory->first > rowIndex))
        {
            break;  // found it!
        }

        ++categoryIter;
    }

    // sanity check. category must exist, and requested index can't be a category.
    if ((categoryIter == end) || (categoryIter->first == rowIndex))
    {
        return ItemRef();
        //throw InvalidSourcesItemException();
    }

    // dig up the actual item
    int offsetInCategory = (int) (rowIndex - categoryIter->first - 1);
    if (offsetInCategory < 0)
    {
        return ItemRef();
        //throw InvalidSourcesItemException();
    }
    //
    return (categoryIter->second->ItemAt(offsetInCategory));
}

int                 SourcesListModel::VisibleItemCount()
{
    int count = 0;
    CategoryList::iterator it = this->categories.begin();
    while (it != this->categories.end())
    {
        count += (1 + (*it)->Count());
        ++it;
    }

    return count;
}

#define FIND_CATEGORY(list, category) \
    std::find(list.begin(), list.end(), category) \

#define CATEGORY_ALREADY_EXISTS(list, category) \
    (FIND_CATEGORY(list, category) != list.end())

CategoryRef        SourcesListModel::AddCategory(CategoryRef category)
{
    if (CATEGORY_ALREADY_EXISTS(this->categories, category))
    {
        throw InvalidSourcesCategoryException();
    }

    this->categories.push_back(category);
    this->IndexCategories();
    this->SetRowCount(this->VisibleItemCount());

    return category;
}

CategoryRef        SourcesListModel::RemoveCategory(CategoryRef category)
{
    CategoryList::iterator it = FIND_CATEGORY(this->categories, category);
    if (it != this->categories.end())
    {
        it = this->categories.erase(it);
        this->IndexCategories();
        this->SetRowCount(this->VisibleItemCount());

        return *it;
    }

    throw InvalidSourcesCategoryException();
}

void                SourcesListModel::IndexCategories()
{
    this->categoryIndexMap.clear();

    int position = 0;
    CategoryList::iterator it = this->categories.begin();
    while (it != this->categories.end())
    {
        categoryIndexMap[position] = *it;
        position += (1 + (*it)->Count());
        ++it;
    }

    this->DataChanged(-1);
}

void                SourcesListModel::SelectedRowChanged(int newIndex)
{
    ItemRef itemAtIndex = this->ItemAtRowIndex(newIndex);
    if (itemAtIndex && (this->activeItem != itemAtIndex))
    {
        this->activeItemIndex = newIndex;
        this->activeItem = itemAtIndex;
        //
        this->ActiveItemChanged(itemAtIndex);
    }
}

//////////////////////////////////////////////////////////////////////////////
// SourcesListModel::CategoryCellRenderer
//////////////////////////////////////////////////////////////////////////////

FontRef SourcesListModel::CategoryCellRenderer::sFont = Font::Create(_T(""), -1, true);

/*ctor*/    SourcesListModel::CategoryCellRenderer::CategoryCellRenderer(const uistring& textValue)
: base(textValue)
{
}

void        SourcesListModel::CategoryCellRenderer::SetFonts(FontRef font)
{
    CategoryCellRenderer::sFont = font;
}

void        SourcesListModel::CategoryCellRenderer::Render(const ListView& listView, ListView::RenderParams& renderParams)
{
    renderParams.backColor = Color::Darken(Color::SystemColor(COLOR_BTNFACE), 15);
    renderParams.foreColor = Color::SystemColor(COLOR_BTNTEXT);

    HBRUSH backBrush = ::CreateSolidBrush(renderParams.backColor);
    ::FillRect(renderParams.hdc, &(RECT) renderParams.rect, backBrush);
    ::DeleteObject(backBrush);

    ::SetTextColor(renderParams.hdc, renderParams.foreColor);
    ::SetBkMode(renderParams.hdc, TRANSPARENT);

    RECT drawRect = renderParams.rect;
    drawRect.left += 6;

    SourcesListModel::CategoryCellRenderer::sFont->DrawToHDC(
        renderParams.hdc, drawRect, this->textValue);
}

//////////////////////////////////////////////////////////////////////////////
// SourcesListModel::ItemCellRenderer
//////////////////////////////////////////////////////////////////////////////

/*ctor*/    SourcesListModel::ItemCellRenderer::ItemCellRenderer(SourcesListModel& owner, const uistring& textValue)
: base(textValue)
, owner(owner)
{
}

void        SourcesListModel::ItemCellRenderer::Render(const ListView& listView, ListView::RenderParams& renderParams)
{
    static const int gutterWidth = 6;

    bool isSelected = false;
    bool isHot = (listView.HotRow() == renderParams.rowIndex);
    Color gutterColor = Color::SystemColor(COLOR_BTNFACE);
    FontRef font = Font::Create();

    // selected
    if (renderParams.rowIndex == owner.activeItemIndex)
    {
        isSelected = true;

        renderParams.foreColor = Color::SystemColor(COLOR_HIGHLIGHTTEXT);
        renderParams.backColor = Color::SystemColor(COLOR_HIGHLIGHT);
        gutterColor = Color::SystemColor(COLOR_ACTIVECAPTION);

        font->SetBold();
    }
    else
    {
        renderParams.backColor = Color::Lighten(Color::SystemColor(COLOR_BTNFACE), 15);
        renderParams.foreColor = Color::SystemColor(COLOR_BTNTEXT);
    }

    // fill the background
    if (isHot)
    {
        renderParams.backColor = Color::Lighten(renderParams.backColor, 15);
    }
    //
    HBRUSH backBrush = ::CreateSolidBrush(renderParams.backColor);
    ::FillRect(renderParams.hdc, &(RECT) renderParams.rect, backBrush);
    ::DeleteObject(backBrush);

    // fill the gutter
    RECT gutterRect = renderParams.rect;
    gutterRect.right = gutterRect.left + gutterWidth;
    backBrush = ::CreateSolidBrush(gutterColor);
    ::FillRect(renderParams.hdc, &gutterRect, backBrush);
    ::DeleteObject(backBrush);

    // draw the caption
    ::SetTextColor(renderParams.hdc, renderParams.foreColor);
    ::SetBkMode(renderParams.hdc, TRANSPARENT);
    //
    renderParams.rect.location.x += gutterWidth + 4;
    //
    font->DrawToHDC(renderParams.hdc, renderParams.rect, this->textValue);
}