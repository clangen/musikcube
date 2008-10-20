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

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Window.hpp>
#include <win32cpp/Win32Exception.hpp>
#include <win32cpp/Color.hpp>

#include <boost/format.hpp>
#include <boost/scoped_ptr.hpp>

#include <vector>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// ListView
//////////////////////////////////////////////////////////////////////////////

///\brief
///ScrollBar is an enumeration that describles the orientation of a scrollbar.
enum ScrollBar
{
    HorizontalScrollBar = SB_HORZ,  /*!< */ 
    VerticalScrollBar = SB_VERT,    /*!< */ 
    BothScrollBars = SB_BOTH        /*!< */ 
};


///\brief
///Defines the a ListView's default row height.
///
///\see
///ListView::RowHeight, ListView::SetRowHeight
const int DefaultRowHeight = -1;

///\brief
///A ListView is used for displaying data in row/column format.
///
///The ListView class wraps a Win32 WC_LISTVIEW with LVS_OWNERDATA; all data is
///virtualized. In other words, data isn't stuffed into the view. Rather,
///the ListView asks a ListView::Model to supply it with data as needed.
///This makes displaying large amounts of data extremely efficient.
///
///ListView also supports custom drawing via CellRenderer and RowRenderer
///objects. The model can optionally supply custom renderers to easily
///change the appearance of the view.
///
///\see
///Window
class ListView: public Window
{
public: // types
    /*! */ class InvalidColumnException: public Exception { };
    /*! */ class InvalidSelectionTypeException: public Exception { };

    class Column;
    class RowRenderer;
    /*abstract*/ class CellRenderer;
    /*abstract*/ class Model;
    template <typename T> class TextCellRenderer;

    /*! */ typedef std::vector<int> RowIndexList;

    ///\brief A shared pointer to a ListView::Column
    typedef boost::shared_ptr<Column> ColumnRef;
    ///\brief A shared pointer to a ListView::CellRenderer
    typedef boost::shared_ptr<CellRenderer> CellRendererRef;
    ///\brief A shared pointer to a ListView::RowRenderer
    typedef boost::shared_ptr<RowRenderer> RowRendererRef;
    ///\brief A shared pointer to a ListView::Model
    typedef boost::shared_ptr<Model> ModelRef;

    /*! */ typedef sigslot::signal2<ListView* /*sender*/, int /*row*/> HotRowChangedEvent;
    /*! */ typedef sigslot::signal2<ListView* /*sender*/, int /*row*/> RowActivatedEvent;
    /*! */ typedef sigslot::signal3<ListView* /*sender*/, int /*row*/, ColumnRef> HotCellChangedEvent;
    /*! */ typedef sigslot::signal1<ListView* /*sender*/> SelectionChangedEvent;
    /*! */ typedef sigslot::signal2<ListView*, ColumnRef> ColumnClickedEvent;
    /*! */ typedef sigslot::signal2<ListView*, MenuRef> PrepareContextMenuEvent;

    struct RenderParams
    {
        int rowIndex;
        ColumnRef column;
        Color backColor;
        Color foreColor;
        Rect rect;
        HDC hdc;
    };

private: // types
    typedef Window base;
    typedef std::map<ColumnRef, int> ColumnToIndexMap;
    class NullModel;

public: // events
    ///\brief Emitted when the the mouse cursor has entered a new row
    HotRowChangedEvent      HotRowChanged;
    ///\brief Emitted when the the mousebutton doubleclicks on a row
    RowActivatedEvent       RowActivated;
    ///\brief Emitted when the the mouse cursor has entered a new cell
    HotCellChangedEvent     HotCellChanged;
    ///\brief Emitted when the selection has changed
    SelectionChangedEvent   SelectionChanged;
    ///\brief Emitted when a Column is clicked
    ColumnClickedEvent      ColumnClicked;
    ///\brief Emitted immediately prior to showing the Context menu. listeners
    ///can use this event ot add/remove/update menu items
    PrepareContextMenuEvent PrepareContextMenu;

public: // constructors
    /*ctor*/        ListView();

public: // methods
    void            AddColumn(ColumnRef column);
    void            RemoveColumn(ColumnRef column);
    int             ColumnWidth(ColumnRef column) const;
    bool            SetColumnWidth(ColumnRef column, int width);
    void            EnableColumnResizing(bool enable);
    bool            ResizableColumnsEnabled() const;
    void            EnableStripedBackground(bool enable);
    bool            StripedBackgroundEnabled() const;
    void            EnableMultipleSelection(bool enable);
    bool            MultipleSelectionEnabled() const;
    void            SetScrollBarVisibility(ScrollBar scrollBar, bool isVisible);
    void            SetRowHeight(int height = DefaultRowHeight);
    int             RowHeight() const;
    void            SetModel(ModelRef model);
    void            SetColumnsVisible(bool visible);
    bool            ColumnsVisible() const;
    void            ScrollToRow(int rowIndex);
    int             RowAtPoint(const Point& location) const;
    bool            CellAtPoint(const Point& location, int& rowIndex, ColumnRef& column) const;
    void            RedrawRow(int rowIndex) const;
    void            RedrawCell(int rowIndex, ColumnRef column) const;
    int             HotRow() const;
    ColumnRef       HotColumn() const;
    RowIndexList    SelectedRows();
    int             SelectedRow();
    void            SelectRows(const std::vector<int>& indices);
    void            SelectRow(int index);
    void            DeselectRows(const std::vector<int>& indices);
    void            DeselectRow(int index);
    void            SetContextMenu(MenuRef contextMenu);

protected: // methods
    virtual HWND        Create(Window* parent);
    virtual LRESULT     PreWindowProc(UINT message, WPARAM wParam, LPARAM lParam, bool& discardMessage);
    virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void        DrawRow(DRAWITEMSTRUCT* drawInfo);
    ColumnRef           ColumnIndexToColumnRef(int index) const;
    int                 ColumnRefToColumnIndex(ColumnRef column) const;
    Rect                CellRect(int rowIndex, int columnIndex) const;
    Rect                CellRect(int rowIndex, ColumnRef column) const;
    Rect                RowRect(int rowIndex) const;
    void                SetHotCell(int rowIndex, ColumnRef column);
    void                IndexSelectedRows();
    virtual void        OnSelectionChanged();

    // Window overrides
    virtual void        OnEraseBackground(HDC hdc);
    virtual void        OnPaint();
    virtual void        OnRowCountChanged();
    virtual void        OnDataChanged(int rowIndex);
    virtual void        OnMeasureItem(MEASUREITEMSTRUCT* measureItemStruct);
    virtual void        OnMouseMoved(MouseEventFlags flags, const Point& location);
    virtual void        OnRowActivated(int rowIndex);
    virtual LRESULT     OnCustomDraw(NMLVCUSTOMDRAW* customDraw);
    void                OnMouseExit();

protected: // instance data
    ColumnToIndexMap columnToIndexMap;
    bool stripedBackground, multipleSelection;
    bool horizScrollBarVisible, vertScrollBarVisible;
    bool columnsResizable, columnsVisible;
    ModelRef model;
    HWND headerHandle;
    mutable int rowHeight, originalRowHeight;
    int hotRowIndex;
    ColumnRef hotColumn;
    RowIndexList selectedRows;
    int selectedRowIndex;
    bool selectedRowsDirty;
    MenuRef contextMenu;

protected: // class data
    static ModelRef sNullModel;
};

//////////////////////////////////////////////////////////////////////////////
// ListView::Column
//////////////////////////////////////////////////////////////////////////////

///\brief
///Represents a Column of data in a ListView.
///
///Columns are typically passed around in the form of a ColumnRef, a 
///shared pointer to a Column. It is best to create them as follows:
///
///\code
///ColumnRef myColumn(new Column(_T("My Column")));
///\endcode
///
///\see
///ListView::ColumnRef
class ListView::Column
{
protected: // constructors
    ///\brief Constructor
    ///
    ///\param name Name of the column.
    ///\param defaultWidth The default width of the column, in pixels
    ///\param alignment The Column's text alignment
    /*ctor*/            Column(const uichar* name, int defaultWidth = 100, TextAlignment alignment = TextAlignLeft)
    : name(name)
    , defaultWidth(defaultWidth)
    , alignment(alignment)
    {
    }

public: // create methods
    static ColumnRef    Create(const uichar* name, int defaultWidth = 100, TextAlignment alignment = TextAlignLeft)
    {
        return ColumnRef(new Column(name, defaultWidth, alignment));
    }

public: // methods
    ///\brief Returns the display name of the column
    virtual const uichar*   Name() { return this->name.c_str(); }
    ///\brief Returns default display width of the column
    virtual int             DefaultWidth() { return this->defaultWidth; }
    ///\brief Returns requested text alignment of the column
    virtual TextAlignment   Alignment() { return this->alignment; }

protected: // instance data
    int defaultWidth;
    uistring name;
    TextAlignment alignment;
};

//////////////////////////////////////////////////////////////////////////////
// ListView::RowRenderer
//////////////////////////////////////////////////////////////////////////////

///\brief
///Used for rendering rows in the ListView.
///
///RowRenderers typically prepare the background for CellRenderers, e.g.
///by drawing alternating background color. CellRenderers are used to
///draw the contents of a specific (rowIndex, Column) pair.
///
///The default RowRenderer just fills the the background with
///renderParams.backColor.
///
///RowRenderers are usually passed by shared pointer (RowRendererRef), e.g.
///\code
///RowRendererRef rowRenderRef(new MyRowRenderer());
///\endcode
///
///\see
///ListView::CellRenderer
class ListView::RowRenderer
{
public: // methods
    ///\brief
    ///Render the background of listView to with the parameters of renderParams.
    ///
    ///\param listView
    ///The parent ListView.
    ///
    ///\param renderParams
    ///The suggested draw parameters.
    virtual void Render(const ListView& listView, RenderParams& renderParams);
};

//////////////////////////////////////////////////////////////////////////////
// ListView::CellRenderer
//////////////////////////////////////////////////////////////////////////////

///\brief
///Used for rendering a (rowIndex, Column) pair.
///
///The default CellRenderer, TextCellRenderer, renders any class that
///supports the output stream operator <<
///
///CellRenderers are usually passed by shared pointer (CellRendererRef), e.g.
///\code
///CellRendererRef cellRenderer(new MyCellRenderer());
///\endcode
///
///\see
///ListView::RowRenderer
class ListView::CellRenderer
{
public: // methods
    ///\brief
    ///Render the cell with the parameters of renderParams.
    ///
    ///\param listView
    ///The parent ListView.
    ///
    ///\param renderParams
    ///The suggested draw parameters.
    virtual void Render(const ListView& listView, RenderParams& renderParams) = 0;
};

//////////////////////////////////////////////////////////////////////////////
// ListView::TextCellRenderer
//////////////////////////////////////////////////////////////////////////////

///\brief
///The default CellRenderer.
///
///Renders any class that supports the output stream operator <<
///
///\see
///ListView::CellRenderer
template <typename T>
class ListView::TextCellRenderer : public ListView::CellRenderer
{
public: // constructors
    ///\brief
    ///Constructor.
    ///
    ///\param value The value to render
    ///\param alignment The alignment to use
    /*ctor*/        TextCellRenderer(const T& value, TextAlignment alignment = TextAlignLeft);

public: // methods
    void    Render(const ListView& listView, RenderParams& renderParams);

protected: // instance data
    uistring textValue;
    TextAlignment alignment;
};

template <typename T>
/*ctor*/    ListView::TextCellRenderer<T>::TextCellRenderer(const T& value, TextAlignment alignment)
: alignment(alignment)
{
    typedef boost::basic_format<uichar> format;
    this->textValue = (format(_T("%1%")) % value).str();
}

template <typename T>
void        ListView::TextCellRenderer<T>::Render(const ListView& listView, RenderParams& renderParams)
{
    ::SetTextColor(renderParams.hdc, renderParams.foreColor);
    ::SetBkMode(renderParams.hdc, TRANSPARENT);

    DRAWTEXTPARAMS drawTextParams;
    ::SecureZeroMemory(&drawTextParams, sizeof(DRAWTEXTPARAMS));
    drawTextParams.cbSize = sizeof(DRAWTEXTPARAMS);
    drawTextParams.iLeftMargin = 6;
    //
    int bufferSize = (int) this->textValue.size() + 4;  // DrawTextEx may add up to 4 additional characters
    boost::scoped_ptr<uichar> buffer(new uichar[bufferSize]);
    ::wcsncpy_s(buffer.get(), bufferSize, this->textValue.c_str(), this->textValue.size());
    //
    RECT drawRect = renderParams.rect;
    //
    ::DrawTextEx(
        renderParams.hdc,
        buffer.get(),
        -1, // buffer is NULL terminated
        &drawRect,
        this->alignment | DT_END_ELLIPSIS | DT_VCENTER | DT_SINGLELINE,
        &drawTextParams);
}

//////////////////////////////////////////////////////////////////////////////
// ListView::Model
//////////////////////////////////////////////////////////////////////////////

///\brief
///Supplies one or more ListView instances with data to display.
///
///The user must subclass Model and override Model::CellValueToString
///to get data into the ListView.
///
///\code
///class MyModel: public ListView::Model
///{
///public:
///    virtual uistring CellValueToString(int rowIndex, ColumnRef column)
///    {
///        // a real implementation would do something more useful!
///        return _T("value at rowIndex, column");
///    }
///};
///
///class MyController
///{
///private:
///    //...
///    void OnViewCreated();
///    ListView::ModelRef model;
///    ListView& listView;
///};
///
///void MyController::OnViewCreated()
///{
///    //...
///    // Create an instance of MyModel and give it to the view.
///    this->model.reset(new MyModel());
///    this->listView.SetModel(this->model);
///
///    // SetRowCount will emit a RowCountChanged event, which the
///    // ListView listens for. Once the row count is set, the ListView
///    // will call Model::CellValueToString to display the data.
///    this->model->SetRowCount(60);
///    //...
///};
///\endcode
///
///
///Model instances are usually passed by shared pointer, so it's best to
///create and use them as follows:
///
///\code
///ModelRef myModel(new MyModel());
///\endcode
///
///A single ListView::ModelRef can be shared amongst multiple ListView instances.
///
///A Model can also be used to (optionally) provide the ListView
///with custom RowRenderer and CellRenderer instances to drastically
///change the appearance of the view.
///
///\see
///ListView, ListView::RowRenderer, ListView::CellRenderer
/*abstract*/ class ListView::Model
{
public: // types
    /*! */ typedef sigslot::signal0<> RowCountChangedEvent;
    /*! */ typedef sigslot::signal1<int> DataChangedEvent;

public: // events
    /*!\brief Emitted when the number of rows has changed. */
    RowCountChangedEvent RowCountChanged;
    /*!\brief Emitted when data in the model has been updated. */
    DataChangedEvent DataChanged;

public: // constructors

    ///\brief Constructor
    ///\param rowCount The number of rows to initialize the Model with.
    /*ctor*/ Model(int rowCount = 0)
    : rowCount(rowCount)
    {
    }

public: // methods
    ///\brief Returns the number of rows in the Model.
    int RowCount()
    {
        return this->rowCount;
    }

    ///\brief Returns a CellRenderer for the specified (rowIndex, Column)
    ///
    ///If no CellRenderer is returned the ListView will use a TextCellRenderer
    ///with the value obtained by Model::CellValueToString
    ///
    ///\param rowIndex
    ///The row index of interest.
    ///
    ///\param column
    ///The Column of the row.
    ///
    ///\returns
    ///A CellRenderer for the specified (rowIndex, Column)
    ///
    ///\see
    ///Model::RowRenderer, Model::CellValueToString.
    virtual CellRendererRef CellRenderer(int rowIndex, ColumnRef column)
    {
        return CellRendererRef();
    }

    ///\brief Returns a RowRenderer for the specified row index
    ///
    ///If no RowRenderer is returned the ListView will fill the row with
    ///the default background color.
    ///
    ///\param rowIndex
    ///The row index of interest.
    ///
    ///\returns
    ///A RowRenderer for the specified row index
    ///
    ///\see
    ///Model::CellRenderer, Model::CellValueToString.
    virtual RowRendererRef RowRenderer(int rowIndex)
    {
        return RowRendererRef(new ListView::RowRenderer());
    }

    ///\brief Returns a string representation of the value at (rowIndex, Column)
    ///
    ///This is the only pure virtual method of the class, and must be overriden.
    ///
    ///\param rowIndex
    ///The rowIndex.
    ///
    ///\param column
    ///The Column.
    ///
    ///\returns
    ///A string representation of the value at (rowIndex, Column)
    virtual uistring CellValueToString(int rowIndex, ColumnRef column) = 0;

protected: // methods
    ///\brief Sets the number of rows in the Model.
    ///
    ///This method will cause RowCountChanged to be emitted.
    ///
    ///\param rowCount The new number of rows.
    void SetRowCount(int rowCount)
    {
        if (rowCount != this->rowCount)
        {
            this->rowCount = rowCount;
            this->RowCountChanged();
        }
    }

    void InvalidateData(int forRow = -1)
    {
        this->DataChanged(forRow);
    }

private: // instance data
    int rowCount;
};

//////////////////////////////////////////////////////////////////////////////
// ListView::NullModel
//////////////////////////////////////////////////////////////////////////////

class ListView::NullModel : public ListView::Model
{
public: // methods
    virtual int RowCount()
    {
        return 0;
    }

    virtual uistring 
    CellValueToString(int rowIndex, ListView::ColumnRef column)
    {
        return uistring();
    }
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
