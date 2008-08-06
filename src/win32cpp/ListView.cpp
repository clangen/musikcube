//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
//
// Sources and Binaries of: win32cpp
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
#include <win32cpp/ListView.hpp>

#include <wingdi.h>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////
// ListView
//////////////////////////////////////////////////////////////////////////////

ListView::ModelRef ListView::sNullModel(new ListView::NullModel());

///\brief
///Default constructor
/*ctor*/    ListView::ListView()
: base()
, stripedBackground(false)
, horizScrollBarVisible(true)
, vertScrollBarVisible(true)
, columnsResizable(true)
, columnsVisible(true)
, selectedRowsDirty(true)
, model(ListView::sNullModel)
, headerHandle(NULL)
, rowHeight(DefaultRowHeight)
, originalRowHeight(-1)
, multipleSelection(true)
, hotRowIndex(-1)
, selectedRowIndex(-1)
, hotColumn()
{
}

HWND        ListView::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance();
    parent = (parent ? parent : Application::Instance().MainWindow());

    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS
                | LVS_REPORT | LVS_OWNERDATA | LVS_OWNERDRAWFIXED;

    if ( ! this->multipleSelection)
    {
        style |= LVS_SINGLESEL;
    }

    // create the window
    HWND hwnd = CreateWindowEx(
        NULL,                       // ExStyle
        WC_LISTVIEW,                // Class name
        _T(""),                     // Window name
        style,                      // Style
        0,                          // X
        0,                          // Y
        120,                        // Width
        36,                         // Height
        parent->Handle(),           // Parent
        NULL,                       // Menu
        hInstance,                  // Instance
        NULL);                      // lParam

    if (hwnd)
    {
        DWORD styleEx = LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP | LVS_EX_DOUBLEBUFFER;

        ListView_SetExtendedListViewStyle(hwnd, styleEx);
        ListView_SetItemCountEx(hwnd, this->model->RowCount(), LVSICF_NOSCROLL);

        this->headerHandle = ListView_GetHeader(hwnd);

        this->SetBackgroundColor(Color::SystemColor(COLOR_WINDOW));
    }

    return hwnd;
}

LRESULT     ListView::PreWindowProc(UINT message, WPARAM wParam, LPARAM lParam, bool& discardMessage)
{
    // Hack to make marquee selection work with the LVS_OWNERDRAWFIXED style. Disable
    // the style when the mouse button is down, re-enable it when the button is released.
    switch (message)
    {
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        {
            DWORD style = ::GetWindowLong(this->Handle(), GWL_STYLE);
            ::SetWindowLong(this->Handle(), GWL_STYLE, style & ~LVS_OWNERDRAWFIXED);
        }
        break;

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        {
            DWORD style = ::GetWindowLong(this->Handle(), GWL_STYLE);
            ::SetWindowLong(this->Handle(), GWL_STYLE, style | LVS_OWNERDRAWFIXED);
        }
        break;
    }

    return 0;
}

LRESULT     ListView::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_NOTIFY:
        {
            if ( ! lParam)
            {
                break;
            }

            NMHDR* notifyHeader = reinterpret_cast<NMHDR*>(lParam);
            switch (notifyHeader->code)
            {
            case NM_CUSTOMDRAW:
                {
                    // The header ctrl also emits NM_CUSTOMDRAW messages
                    if (notifyHeader->hwndFrom == this->Handle())
                    {
                        NMLVCUSTOMDRAW* lvCustomDraw = reinterpret_cast<NMLVCUSTOMDRAW*>(notifyHeader);
                        return this->OnCustomDraw(lvCustomDraw);
                    }
                }
                break;

            case NM_RCLICK:
                {
                    POINT mousePos = { 0 };
                    ::GetCursorPos(&mousePos);
                    if (this->contextMenu)
                    {
                        ::TrackPopupMenu(
                            this->contextMenu->Handle(),
                            NULL,
                            mousePos.x,
                            mousePos.y,
                            NULL,
                            this->Handle(),
                            NULL);
                    }
                }
                break;

            case HDN_BEGINTRACK:
                {
                    return ( ! this->columnsResizable);
                }

            case HDN_DIVIDERDBLCLICK:
                {
                    return 0;
                }

            case HDN_ITEMCLICK:
                {
                    NMHEADER* clickHeader = reinterpret_cast<NMHEADER*>(notifyHeader);
                    this->ColumnClicked(this, this->ColumnIndexToColumnRef(clickHeader->iItem));
                }
                break;

            case LVN_ITEMCHANGED:       // single item has changed
            case LVN_ODSTATECHANGED:    // range of items has changed
                {
                    NMLISTVIEW* nmListView = reinterpret_cast<NMLISTVIEW*>(lParam);

                    if (nmListView && (nmListView->iItem != -1))
                    {
                        bool isSelected = ((nmListView->uNewState & LVIS_SELECTED) != 0);
                        bool wasSelected = ((nmListView->uOldState & LVIS_SELECTED) != 0);

                        if (isSelected != wasSelected)
                        {
                            this->OnSelectionChanged();
                        }
                    }
                }
                break;

            case LVN_ITEMACTIVATE:
                {
                    NMITEMACTIVATE* itemActivate = reinterpret_cast<NMITEMACTIVATE*>(lParam);

                    if (itemActivate->iItem > -1)
                    {
                        this->OnRowActivated(itemActivate->iItem);
                    }
                }
                return 0;

            case LVN_MARQUEEBEGIN:
                return 0;
            }
        }
        break;

    case WM_NCCALCSIZE:
        {
            DWORD style = ::GetWindowLong(this->Handle(), GWL_STYLE);
            if ( ! this->horizScrollBarVisible) style &= ~WS_HSCROLL;
            if ( ! this->vertScrollBarVisible) style &= ~WS_VSCROLL;
            //
            ::SetWindowLong(this->Handle(), GWL_STYLE, style);
        }
        break;
    }

    return base::WindowProc(message, wParam, lParam);
}

LRESULT     ListView::OnCustomDraw(NMLVCUSTOMDRAW* customDraw)
{
    static const DWORD SUBITEM_PREPAINT = (CDDS_ITEMPREPAINT | CDDS_SUBITEM);

    switch (customDraw->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        {
            return CDRF_NOTIFYITEMDRAW;
        }

    case CDDS_ITEMPREERASE:
        {
            return CDRF_SKIPDEFAULT;
        }

    case CDDS_ITEMPREPAINT: // we drow the row and all of the cells here!
        {
            DRAWITEMSTRUCT drawItemStruct;
            ::SecureZeroMemory(&drawItemStruct, sizeof(drawItemStruct));
            drawItemStruct.itemID = (UINT) customDraw->nmcd.dwItemSpec;
            drawItemStruct.hDC = customDraw->nmcd.hdc;
            drawItemStruct.rcItem = this->RowRect(drawItemStruct.itemID);

            this->DrawRow(&drawItemStruct);

            return CDRF_SKIPDEFAULT;
        }

    case SUBITEM_PREPAINT:
        {
            return CDRF_SKIPDEFAULT;
        }

    default:
        {
            return CDRF_DODEFAULT;
        }
    }
}

void        ListView::OnSelectionChanged()
{
    this->selectedRowsDirty = true;
    this->SelectionChanged(this);
}

void        ListView::IndexSelectedRows()
{
    if (this->selectedRowsDirty)
    {
        this->selectedRows.clear();
        this->selectedRowIndex = -1;

        LVITEM item;
        SecureZeroMemory(&item, sizeof(LVITEM));
        //
        for (int i = 0; i < this->model->RowCount(); i++)
        {
            item.mask = LVIF_STATE;
            item.stateMask = LVIS_SELECTED;
            item.iItem = i;

            if ( ! ListView_GetItem(this->Handle(), &item))
            {
                throw Win32Exception();
            }

            if (item.state & LVIS_SELECTED)
            {
                if (this->multipleSelection)
                {
                    this->selectedRows.push_back(i);
                }
                else
                {
                    this->selectedRowIndex = i;
                    return;
                }
            }
        }

        this->selectedRowsDirty = false;
    }
}

Rect        ListView::CellRect(int rowIndex, ColumnRef column) const
{
    return this->CellRect(rowIndex, this->ColumnRefToColumnIndex(column));
}

Rect        ListView::CellRect(int rowIndex, int columnIndex) const
{
    RECT cellRect;

    ListView_GetSubItemRect(
        this->Handle(),
        rowIndex,
        (LONG) columnIndex,
        LVIR_BOUNDS,
        &cellRect);

    // the width reported by ListView_GetSubItemRect can be inaccurate, so
    // validate it against the header's width.
    RECT headerRect;
    Header_GetItemRect(this->headerHandle, columnIndex, &headerRect);

    cellRect.left = headerRect.left;
    cellRect.right = headerRect.right;

    return cellRect;
}


Rect        ListView::RowRect(int rowIndex) const
{
    RECT result;
    ListView_GetItemRect(this->Handle(), rowIndex, &result, LVIR_BOUNDS);
    return result;
}

void        ListView::DrawRow(DRAWITEMSTRUCT* itemInfo)
{
    // figure out if the row is selected or not.
    LVITEM item;
    SecureZeroMemory(&item, sizeof(LVITEM));
    item.mask = LVIF_STATE;
    item.stateMask = LVIS_SELECTED;
    item.iItem = itemInfo->itemID;
    //
    if ( ! ListView_GetItem(this->Handle(), &item))
    {
        throw Win32Exception();
    }
    //
    bool selected = ((item.state & LVIS_SELECTED) != 0);

    // initialize RenderParams
    RenderParams renderParams = { 0 };
    renderParams.rowIndex = (int) itemInfo->itemID;
    renderParams.hdc = itemInfo->hDC;
    renderParams.rect = itemInfo->rcItem;
    renderParams.foreColor = ::GetSysColor(COLOR_WINDOWTEXT);
    renderParams.backColor = ::GetSysColor(COLOR_WINDOW);
    //
    if (selected)
    {
        renderParams.foreColor = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
        renderParams.backColor = ::GetSysColor(COLOR_HIGHLIGHT);
    }
    else if ((this->stripedBackground) && (renderParams.rowIndex % 2 == 0))
    {
        renderParams.foreColor = Color::Darken(renderParams.foreColor, 15);
        renderParams.backColor = Color::Darken(renderParams.backColor, 15);
    }

    // Render the row (usually just the background)
    RowRendererRef rowRenderer = this->model->RowRenderer(renderParams.rowIndex);
    if (rowRenderer)
    {
        rowRenderer->Render(*this, renderParams);
    }

    // Iterate over all of the cells in this row and draw them
    for (size_t cellIndex = 0; cellIndex < this->columnToIndexMap.size(); cellIndex++)
    {
        renderParams.column = this->ColumnIndexToColumnRef((int) cellIndex);
        renderParams.rect = this->CellRect(renderParams.rowIndex, (int) cellIndex);

        // Allow the model to specify a custom cell renderer
        CellRendererRef cellRenderer = 
            this->model->CellRenderer(renderParams.rowIndex, renderParams.column);

        if ( ! cellRenderer)
        {
            // If the model didn't specify a custom cell renderer then try to
            // render it as text.
            uistring cellText =
                this->model->CellValueToString(renderParams.rowIndex, renderParams.column);

            if (cellText.size())
            {
                typedef TextCellRenderer<uistring> TextRenderer;
                //
                cellRenderer = CellRendererRef(
                    new TextRenderer(cellText, renderParams.column->Alignment()));
            }
        }
        //
        if (cellRenderer)
        {
            cellRenderer->Render(*this, renderParams);
        }
    }
}

void        ListView::OnPaint()
{
    base::DefaultWindowProc(WM_PAINT, NULL, NULL);
}

void        ListView::OnEraseBackground(HDC hdc)
{
    ListView_SetBkColor(this->Handle(), this->BackgroundColor());
    base::DefaultWindowProc(WM_ERASEBKGND, (WPARAM) hdc, NULL);
}

void        ListView::OnRowCountChanged()
{
    if (this->Handle())
    {
        ListView_SetItemCountEx(this->Handle(), this->model->RowCount(), LVSICF_NOSCROLL);
    }
}

void        ListView::OnDataChanged(int rowIndex)
{
    if (this->Handle())
    {
        (rowIndex == -1) ? this->Redraw() : this->RedrawRow(rowIndex);
    }
}

void        ListView::OnMouseMoved(MouseEventFlags flags, const Point& location)
{
    int rowIndex = 0;
    ColumnRef column = ColumnRef();
    //
    if ( ! this->CellAtPoint(location, rowIndex, column))
    {
        rowIndex = -1;
        column = ColumnRef();
    }
    //
    this->SetHotCell(rowIndex, column);
}

void        ListView::OnRowActivated(int rowIndex)
{
    this->RowActivated(this, rowIndex);
}

void        ListView::OnMouseExit()
{
    this->SetHotCell(-1, ColumnRef());
}

void        ListView::OnMeasureItem(MEASUREITEMSTRUCT* measureItemStruct)
{
    if (this->originalRowHeight == -1)
    {
        this->originalRowHeight = measureItemStruct->itemHeight;
    }

    if (this->rowHeight == DefaultRowHeight)
    {
        this->rowHeight = this->originalRowHeight;
    }

    measureItemStruct->itemHeight = this->rowHeight == DefaultRowHeight
        ? this->originalRowHeight
        : this->rowHeight;
}

///\brief
///Adds the specified column to the view
///
///\see ListView::RemoveColumn
void        ListView::AddColumn(ColumnRef column)
{
    if ( ! this->Handle())
    {
        this->Initialize(NULL);
    }

    ColumnToIndexMap::iterator alreadyExists = this->columnToIndexMap.find(column);
    if (alreadyExists != this->columnToIndexMap.end())
    {
        throw InvalidColumnException();
    }

    int alignment = NULL;
    switch (column->Alignment())
    {
    case TextAlignLeft:
        alignment = LVCFMT_LEFT;
        break;
    case TextAlignCenter:
        alignment = LVCFMT_CENTER;
        break;
    case TextAlignRight:
        alignment = LVCFMT_RIGHT;
        break;
    }

    LVCOLUMN newColumn;
    newColumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;
    newColumn.pszText = const_cast<LPWSTR>(column->Name());
    newColumn.cx = column->DefaultWidth();
    newColumn.fmt = alignment;
    newColumn.iSubItem = (int) this->columnToIndexMap.size();

    int columnIndex = ListView_InsertColumn(this->Handle(), newColumn.iSubItem, &newColumn);
    //
    if (columnIndex == -1)
    {
        throw Win32Exception();
    }
    //
    this->columnToIndexMap.insert(
        ColumnToIndexMap::value_type(column, columnIndex));
}

///\brief
///Removes the specified column from the view.
///
///\see ListView::AddColumn
void        ListView::RemoveColumn(ColumnRef column)
{
    ColumnToIndexMap::iterator it = this->columnToIndexMap.find(column);
    if (it == this->columnToIndexMap.end())
    {
        throw InvalidColumnException();
    }

    if ( ! ListView_DeleteColumn(this->Handle(), it->second))
    {
        throw Win32Exception();
    }
    //
    this->columnToIndexMap.erase(it);
}

///\brief
///Returns the width of the specified column.
///
///\param column
///The column to query.
///
///\returns
///The width of the specified column.
///
///\see ListView::SetColumnWidth
int         ListView::ColumnWidth(ColumnRef column) const
{
    ColumnToIndexMap::const_iterator it = this->columnToIndexMap.find(column);
    if (it == this->columnToIndexMap.end())
    {
        throw InvalidColumnException();
    }

    return ListView_GetColumnWidth(this->Handle(), it->second);
}

///\brief
///Sets the width of the specified column.
///
///\param column
///The column to update.
///
///\param width
///The requested width of the column, in pixels.
///
///\returns
///true if successful, false otherwise.
///
///\see
///ListView::ColumnWidth
bool        ListView::SetColumnWidth(ColumnRef column, int width)
{
    ColumnToIndexMap::iterator it = this->columnToIndexMap.find(column);
    if (it == this->columnToIndexMap.end())
    {
        throw InvalidColumnException();
    }

    return (ListView_SetColumnWidth(this->Handle(), it->second, width) == TRUE);
}

///\brief
///Sets the specified scrollbar's visibility.
///
///\param scrollBar
///The scrollbar whose visibility will be changed
///
///\param isVisible
///If true the scrollbar will be visible; if false the scrollbar will be hidden.
///
///\see ScrollBar
void        ListView::SetScrollBarVisibility(ScrollBar scrollBar, bool isVisible)
{
    if ((scrollBar == HorizontalScrollBar) || (scrollBar == BothScrollBars))
    {
        this->horizScrollBarVisible = isVisible;
    }

    if ((scrollBar == VerticalScrollBar) || (scrollBar == BothScrollBars))
    {
        this->vertScrollBarVisible = isVisible;
    }

    this->Resize(this->WindowSize());
}

///\brief
///Enables column resizing via click and drag.
///
///\param enable
///if true column resizing will be enabled.
///
///\see
///ListView::ResizableColumnsEnabled
void        ListView::EnableColumnResizing(bool enable)
{
    this->columnsResizable = enable;
}

///\brief
///Returns true if column resizing is enabled.
///
///\see
///ListView::EnableColumnResizing
bool        ListView::ResizableColumnsEnabled() const
{
    return this->columnsResizable;
}

///\brief
///Enables alternating row background color.
///
///\param enable
///if true alternating row color will be enabled.
///
///\see
///ListView::StripedBackgroundEnabled
void        ListView::EnableStripedBackground(bool enable)
{
    if (enable != this->stripedBackground)
    {
        this->stripedBackground = enable;

        ::InvalidateRect(this->Handle(), NULL, NULL);
    }
}

///\brief
///Returns true if the view alternates row background color
///
///\see
///ListView::EnableStripedBackground
bool        ListView::StripedBackgroundEnabled() const
{
    return this->stripedBackground;
}

///\brief
///Enables multiple row selection.
///
///\param enable
///if true multiple row selection will be enabled.
///
///\see
///ListView::MultipleSelectionEnabled
void        ListView::EnableMultipleSelection(bool enable)
{
    if (this->multipleSelection != enable)
    {
        if (this->Handle())
        {
            DWORD style = ::GetWindowLong(this->Handle(), GWL_STYLE);
            enable ? style &= ~LVS_SINGLESEL : style |= LVS_SINGLESEL;
            //
            ::SetWindowLong(this->Handle(), GWL_STYLE, style);
        }

        this->multipleSelection = enable;
    }
}

///\brief
///Returns true if multiple row selection is enabled
///
///\see
///ListView::EnableMultipleSelection
bool        ListView::MultipleSelectionEnabled() const
{
    return this->multipleSelection;
}

///\brief
///Set the data model used by the ListView.
///
///All displayed data, including ListView::RowRenderer's and
///ListView::CellRenderer's are obtained from the model.
///
///\param model
///A shared pointer to a Listview::Model
///
///\see
///ListView::Model, ListView::ModelRef
void        ListView::SetModel(ModelRef model)
{
    this->model->RowCountChanged.disconnect(this);
    this->model->DataChanged.disconnect(this);
    //
    this->model = model;
    //
    this->model->RowCountChanged.connect(this, &ListView::OnRowCountChanged);
    this->model->DataChanged.connect(this, &ListView::OnDataChanged);

    if (this->Handle())
    {
        ListView_SetItemCountEx(this->Handle(), this->model->RowCount(), LVSICF_NOSCROLL);
    }
}

///\brief
///Scrolls the ListView to assure the item at rowIndex is visible.
///
///\param rowIndex
///The index of the row to scroll to.
void        ListView::ScrollToRow(int rowIndex)
{
    ListView_EnsureVisible(this->Handle(), rowIndex, FALSE);
}

///\brief
///Set the visibility of the column header.
///
///\param visible
///if true the column header will be visible, otherwise it will be hidden.
///
///\see
///ListView::ColumnsVisible
void        ListView::SetColumnsVisible(bool visible)
{
    if (visible != this->columnsVisible)
    {
        DWORD style = ::GetWindowLong(this->Handle(), GWL_STYLE);
        if ( ! visible) style |= LVS_NOCOLUMNHEADER;
        else style &= ~LVS_NOCOLUMNHEADER;
        //
        ::SetWindowLong(this->Handle(), GWL_STYLE, style);

        this->columnsVisible = visible;
    }
}

///\brief
///Returns true if the column headers are visible, false otherwise
///
///\see
///ListView::SetColumnsVisible
bool        ListView::ColumnsVisible() const
{
    return this->columnsVisible;
}

///\brief
///Returns the height of the rows displayed in the ListView
///
///\see
///ListView::SetRowHeight
int         ListView::RowHeight() const
{
    if (this->rowHeight == DefaultRowHeight)
    {
        if (this->Handle())
        {
            Window::ForceMeasureItem(this);
        }
    }

    return this->rowHeight;
}

///\brief
///Sets the height of the rows displayed in the ListView
///
///\param rowHeight
///The height of the row, in pixels
///
///\see
///ListView::RowHeight
void        ListView::SetRowHeight(int rowHeight)
{
    this->rowHeight = rowHeight;

    if (this->Handle())
    {
        Window::ForceMeasureItem(this);
    }
}

///\brief
///Returns the index of the row under the specified Point.
///
///\param location
///The point to test. The coordinates should be relative to the ListView.
///
///\returns
///The index of the row under the specified point.
///
///\see
///ListView::CellAtPoint
int         ListView::RowAtPoint(const Point& location) const
{
    static LVHITTESTINFO hitTestInfo;
    ::SecureZeroMemory(&hitTestInfo, sizeof(LVHITTESTINFO));
    //
    hitTestInfo.pt = location;

    return ListView_HitTest(this->Handle(), &hitTestInfo);
}

///\brief
///Returns the row index and column at the specified Point.
///
///\param location
///The point to test. The coordinates should be relative to the ListView.
///
///\param rowIndex
///An int reference to receive the row index.
///
///\param column
///A ColumnRef reference to receive the ColumnRef.
///
///\returns
///true if the location specified is under a valid row and column.
///
///\see
///ListView::RowAtPoint
bool        ListView::CellAtPoint(const Point& location, int& rowIndex, ColumnRef& column) const
{
    static LVHITTESTINFO hitTestInfo;
    ::SecureZeroMemory(&hitTestInfo, sizeof(LVHITTESTINFO));
    //
    hitTestInfo.pt = location;

    rowIndex = ListView_HitTest(this->Handle(), &hitTestInfo);
    if (rowIndex >= 0)
    {
        // The hit test counts the column header as the first row as well,
        // so make sure we check against this.
        Rect rowRect = this->RowRect(rowIndex);
        if (::PointInRect(location, rowRect))
        {
            ListView_SubItemHitTest(this->Handle(), &hitTestInfo);
            if (hitTestInfo.iSubItem != -1)
            {
                int columnIndex = hitTestInfo.iSubItem;

                column = this->ColumnIndexToColumnRef(columnIndex);
                return true;
            }
        }
    }

    return false;
}

///\brief
///Redraw the row at the specified index.
///
///\param rowIndex
///The index of the row to redraw.
///
///\see
///ListView::RedrawCell
void        ListView::RedrawRow(int rowIndex) const
{
    RECT rowRect = this->RowRect(rowIndex);
    ::InvalidateRect(this->Handle(), &rowRect, FALSE);
}

///\brief
///Redraw the cell at the specified index and Column.
///
///\param rowIndex
///The index of the row to redraw.
///
///\param column
///The Column within the specified row index to redraw.
///
///\see
///ListView::RedrawRow
void        ListView::RedrawCell(int rowIndex, ColumnRef column) const
{
    RECT cellRect = this->CellRect(rowIndex, column);
    ::InvalidateRect(this->Handle(), &cellRect, FALSE);
}

///\brief
///Returns the index of the row currently under the mouse cursor.
///
///\returns
///The index of the row currently under the mouse cursor; -1 if the cursor
///is not under a row.
///
///\see
///ListView::HotColumn
int         ListView::HotRow() const
{
    return this->hotRowIndex;
}

///\brief
///Returns the Column currently under the mouse cursor.
///
///Users should check this value to assure it is valid, as it is possible
///for the mouse to not be under a Column.
///
///\returns
///The Column currently under the mouse cursor.
///
///\see
///ListView::HotColumn
ListView::ColumnRef   ListView::HotColumn() const
{
    return this->hotColumn;
}

///\brief
///Returns a std::vector<int> that contains the selected row indices.
///
///\throws
///InvalidSelectionTypeException if multiple selection is disabled.
///
///\see
///ListView::SelectedRow
ListView::RowIndexList  ListView::SelectedRows()
{
    if ( ! this->multipleSelection)
    {
        throw InvalidSelectionTypeException();
    }

    this->IndexSelectedRows();
    return this->selectedRows;
}

///\brief
///Returns the index of the selected row.
///
///\throws
///InvalidSelectionTypeException if multiple selection is enabled.
///
///\see
///ListView::SelectedRow
int         ListView::SelectedRow()
{
    if (this->multipleSelection)
    {
        throw InvalidSelectionTypeException();
    }

    this->IndexSelectedRows();
    return this->selectedRowIndex;
}

///\brief
///Sets the context menu for the ListView. The context menu is displayed
///whenever the user right clicks inside the ListView.
void        ListView::SetContextMenu(MenuRef contextMenu)
{
    this->contextMenu = contextMenu;
}

void        ListView::SelectRows(const std::vector<int>& indices)
{
    std::vector<int>::const_iterator it = indices.begin();
    for ( ; it != indices.end(); it++)
    {
        ListView_SetItemState(this->Handle(), *it, LVIS_FOCUSED | LVIS_SELECTED, LVIS_SELECTED);
    }
}

void        ListView::SelectRow(int index)
{
    ListView_SetItemState(this->Handle(), index, LVIS_FOCUSED | LVIS_SELECTED, LVIS_SELECTED);
}

void        ListView::DeselectRows(const std::vector<int>& indices)
{
    std::vector<int>::const_iterator it = indices.begin();
    for ( ; it != indices.end(); it++)
    {
        ListView_SetItemState(this->Handle(), *it, 0, LVIS_SELECTED);
    }
}

void        ListView::DeselectRow(int index)
{
    ListView_SetItemState(this->Handle(), index, 0, LVIS_SELECTED);
}

void        ListView::SetHotCell(int rowIndex, ColumnRef column)
{
    bool rowChanged = false, columnChanged = false;
    if (this->hotRowIndex != rowIndex)
    {
        this->hotRowIndex = rowIndex;
        rowChanged = true;
    }

    if (this->hotColumn != column)
    {
        this->hotColumn = column;
        columnChanged = true;
    }

    if (rowChanged)
    {
        this->HotRowChanged(this, this->hotRowIndex);
    }

    if (rowChanged || columnChanged)
    {
        this->HotCellChanged(this, this->hotRowIndex, this->hotColumn);
    }
}

int         ListView::ColumnRefToColumnIndex(ColumnRef column) const
{
    ColumnToIndexMap::const_iterator it = this->columnToIndexMap.find(column);
    if (it == this->columnToIndexMap.end())
    {
        throw InvalidColumnException();
    }

    return it->second;
}

ListView::ColumnRef ListView::ColumnIndexToColumnRef(int index) const
{
    ColumnToIndexMap::const_iterator it = this->columnToIndexMap.begin();
    while (it != this->columnToIndexMap.end())
    {
        if (it->second == index)
        {
            return it->first;
        }
        ++it;
    }

    throw InvalidColumnException();
}

//////////////////////////////////////////////////////////////////////////////
// RowRenderer
//////////////////////////////////////////////////////////////////////////////

void        ListView::RowRenderer::Render(const ListView& listView, RenderParams& renderParams)
{
    HBRUSH bgBrush = ::CreateSolidBrush(renderParams.backColor);
    ::FillRect(renderParams.hdc, & (RECT) renderParams.rect, bgBrush);
    ::DeleteObject(bgBrush);
}
