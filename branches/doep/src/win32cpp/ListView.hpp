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

#include <win32cpp/Window.hpp>
#include <win32cpp/Win32Exception.hpp>
#include <win32cpp/Color.hpp>
#include <win32cpp/Font.hpp>

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
///objects. The model can optionally supply custom renderers to drastically
///change the appearance of the view.
///
///\see
///Window
class ListView: public Window, public EventHandler
{
            //////////////////////////////////////////////
            // Nested classes, types
            //////////////////////////////////////////////

public:     /*! */ class InvalidColumnException: public Exception { };
public:     /*! */ class InvalidSelectionTypeException: public Exception { };

public:     class Column;
public:     class RowRenderer;
public:     /*abstract*/ class CellRenderer;
public:     /*abstract*/ class Model;
public:     template <typename T> class TextCellRenderer;

public:     /*! */ typedef std::vector<int> RowIndexList;

            //////////////////////////////////////////////
            // boost::shared_ptr's to nested classes
            //////////////////////////////////////////////

            ///\brief A shared pointer to a ListView::Column
public:     typedef boost::shared_ptr<Column> ColumnRef;
            ///\brief A shared pointer to a ListView::CellRenderer
public:     typedef boost::shared_ptr<CellRenderer> CellRendererRef;
            ///\brief A shared pointer to a ListView::RowRenderer
public:     typedef boost::shared_ptr<RowRenderer> RowRendererRef;
            ///\brief A shared pointer to a ListView::Model
public:     typedef boost::shared_ptr<Model> ModelRef;

            //////////////////////////////////////////////
            // Signals
            //////////////////////////////////////////////

            
public:     /*! */ typedef sigslot::signal1<int /*row*/> HotRowChangedEvent;
public:     /*! */ typedef sigslot::signal1<int /*row*/> RowDoubleClickEvent;
public:     /*! */ typedef sigslot::signal2<int /*row*/, ColumnRef> HotCellChangedEvent;
public:     /*! */ typedef sigslot::signal0<> SelectionChangedEvent;

            ///\brief Emitted when the the mouse cursor has entered a new row
public:     HotRowChangedEvent      HotRowChanged;
            ///\brief Emitted when the the mousebutton doubleclicks on a row
public:     RowDoubleClickEvent     RowDoubleClick;
            ///\brief Emitted when the the mouse cursor has entered a new cell
public:     HotCellChangedEvent     HotCellChanged;
            ///\brief Emitted when the selection has changed
public:     SelectionChangedEvent   SelectionChanged;

            //////////////////////////////////////////////
            // Implementation
            //////////////////////////////////////////////

public:     /*ctor*/        ListView();

public:     void            AddColumn(ColumnRef column);
public:     void            RemoveColumn(ColumnRef column);
public:     int             ColumnWidth(ColumnRef column) const;
public:     bool            SetColumnWidth(ColumnRef column, int width);
public:     void            EnableColumnResizing(bool enable);
public:     bool            ResizableColumnsEnabled() const;
public:     void            EnableStripedBackground(bool enable);
public:     bool            StripedBackgroundEnabled() const;
public:     void            EnableMultipleSelection(bool enable);
public:     bool            MultipleSelectionEnabled() const;
public:     void            SetScrollBarVisibility(ScrollBar scrollBar, bool isVisible);
public:     void            SetRowHeight(int height = DefaultRowHeight);
public:     int             RowHeight() const;
public:     void            SetModel(ModelRef model);
public:     void            SetColumnsVisible(bool visible);
public:     bool            ColumnsVisible() const;
public:     void            ScrollToRow(int rowIndex);
public:     int             RowAtPoint(const Point& location) const;
public:     bool            CellAtPoint(const Point& location, int& rowIndex, ColumnRef& column) const;
public:     void            RedrawRow(int rowIndex) const;
public:     void            RedrawCell(int rowIndex, ColumnRef column) const;
public:     int             HotRow() const;
public:     ColumnRef       HotColumn() const;
public:     RowIndexList    SelectedRows();
public:     int             SelectedRow();

        // ListView private api
protected:  virtual HWND        Create(Window* parent);
protected:  virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
protected:  ColumnRef           ColumnIndexToColumnRef(int index) const;
protected:  int                 ColumnRefToColumnIndex(ColumnRef column) const;
protected:  Rect                CellRect(int rowIndex, int columnIndex) const;
protected:  Rect                CellRect(int rowIndex, ColumnRef column) const;
protected:  Rect                RowRect(int rowIndex) const;
protected:  void                SetHotCell(int rowIndex, ColumnRef column);
protected:  void                IndexSelectedRows();
protected:  virtual void        OnSelectionChanged();
            // Window overrides
protected:  virtual void        OnEraseBackground(HDC hdc);
protected:  virtual void        OnPaint();
protected:  virtual LRESULT     OnCustomDraw(NMLVCUSTOMDRAW& customDraw);
protected:  virtual void        OnRowCountChanged();
protected:  virtual void        OnDataChanged(int rowIndex);
protected:  virtual void        OnMeasureItem(MEASUREITEMSTRUCT* measureItemStruct);
protected:  virtual void        OnMouseMoved(MouseEventFlags flags, const Point& location);
protected:  virtual void        OnMouseButtonDoubleClicked(MouseEventFlags flags, const Point& location);
protected:  virtual void        OnMouseExit();

private:    typedef Window base;
private:    typedef std::map<ColumnRef, int> ColumnToIndexMap;
private:    class NullModel;

        // instance variables
protected:  ColumnToIndexMap columnToIndexMap;
protected:  bool stripedBackground, multipleSelection;
protected:  bool horizScrollBarVisible, vertScrollBarVisible;
protected:  bool columnsResizable, columnsVisible;
protected:  ModelRef model;
protected:  HWND headerHandle;
protected:  mutable int rowHeight, originalRowHeight;
protected:  int hotRowIndex;
protected:  ColumnRef hotColumn;
protected:  RowIndexList selectedRows;
protected:  int selectedRowIndex;
protected:  bool selectedRowsDirty;
        // class variables
protected:  static ModelRef sNullModel;
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
            ///\brief Constructor
            ///
            ///\param name Name of the column.
            ///\param defaultWidth The default width of the column, in pixels
            ///\param alignment The Column's text alignment
public:     /*ctor*/            Column(const uichar* name, int defaultWidth = 100, TextAlignment alignment = TextAlignLeft)
            : name(name)
            , defaultWidth(defaultWidth)
            , alignment(alignment)
            {
            }

public:     static ColumnRef    Create(const uichar* name, int defaultWidth = 100, TextAlignment alignment = TextAlignLeft)
            {
                return ColumnRef(new Column(name, defaultWidth, alignment));
            }

            ///\brief Returns the display name of the column
public:     virtual const uichar*   Name() { return this->name.c_str(); }

            ///\brief Returns default display width of the column
public:     virtual int             DefaultWidth() { return this->defaultWidth; }

            ///\brief Returns requested text alignment of the column
public:     virtual TextAlignment   Alignment() { return this->alignment; }

protected:  int defaultWidth;
protected:  uistring name;
protected:  TextAlignment alignment;
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
///customDraw.clrTextBk.
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
            ///\brief
            ///Render the background of listView to with the parameters of customDraw.
            ///
            ///\param listView
            ///The parent ListView.
            ///
            ///\param customDraw
            ///The suggested draw parameters. See
            ///http://msdn2.microsoft.com/en-us/library/bb774778.aspx
public:     virtual void Render(const ListView& listView, NMLVCUSTOMDRAW& customDraw);
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
            ///\brief
            ///Render the cell with the parameters of customDraw.
            ///
            ///\param listView
            ///The parent ListView.
            ///
            ///\param customDraw
            ///The suggested draw parameters. See
            ///http://msdn2.microsoft.com/en-us/library/bb774778.aspx
public:     virtual void Render(const ListView& listView, NMLVCUSTOMDRAW& customDraw) = 0;
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
            ///\brief
            ///Constructor.
            ///
            ///\param value The value to render
            ///\param alignment The alignment to use
public:     /*ctor*/        TextCellRenderer(const T& value, TextAlignment alignment = TextAlignLeft);
public:     virtual void    Render(const ListView& listView, NMLVCUSTOMDRAW& customDraw);

protected:  uistring textValue;
protected:  TextAlignment alignment;
};

template <typename T>
/*ctor*/    ListView::TextCellRenderer<T>::TextCellRenderer(const T& value, TextAlignment alignment)
: alignment(alignment)
{
    typedef boost::basic_format<uichar> format;
    this->textValue = (format(_T("%1%")) % value).str();
}

template <typename T>
void        ListView::TextCellRenderer<T>::Render(const ListView& listView, NMLVCUSTOMDRAW& customDraw)
{
    ::SetTextColor(customDraw.nmcd.hdc, customDraw.clrText);
    ::SetBkMode(customDraw.nmcd.hdc, TRANSPARENT);

    DRAWTEXTPARAMS drawTextParams;
    ::SecureZeroMemory(&drawTextParams, sizeof(DRAWTEXTPARAMS));
    drawTextParams.cbSize = sizeof(DRAWTEXTPARAMS);
    drawTextParams.iLeftMargin = 6;
    //
    int bufferSize = (int) this->textValue.size() + 4;  // DrawTextEx may add up to 4 additional characters
    boost::scoped_ptr<uichar> buffer(new uichar[bufferSize]);
    ::wcsncpy_s(buffer.get(), bufferSize, this->textValue.c_str(), this->textValue.size());
    //
    RECT drawRect = customDraw.nmcd.rc;
    //
    ::DrawTextEx(
        customDraw.nmcd.hdc,
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
            /*! */
public:     typedef sigslot::signal0<> RowCountChangedEvent;
            /*!\brief Emitted when the number of rows has changed. */
public:     RowCountChangedEvent RowCountChanged;

            /*! */
public:     typedef sigslot::signal1<int> DataChangedEvent;
            /*!\brief Emitted when data in the model has been updated. */
public:     DataChangedEvent DataChanged;

            ///\brief Constructor
            ///\param rowCount The number of rows to initialize the Model with.
public:     /*ctor*/ Model(int rowCount = 0)
            : rowCount(rowCount)
            {
            }

            ///\brief Returns the number of rows in the Model.
public:     int RowCount()
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
public:     virtual CellRendererRef CellRenderer(int rowIndex, ColumnRef column)
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
public:     virtual RowRendererRef RowRenderer(int rowIndex)
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
public:     virtual uistring CellValueToString(int rowIndex, ColumnRef column) = 0;

            ///\brief Sets the number of rows in the Model.
            ///
            ///This method will cause RowCountChanged to be emitted.
            ///
            ///\param rowCount The new number of rows.
protected:  void SetRowCount(int rowCount)
            {
                if (rowCount != this->rowCount)
                {
                    this->rowCount = rowCount;
                    this->RowCountChanged();
                }
            }

protected:  void InvalidateData(int forRow = -1)
            {
                this->DataChanged(forRow);
            }

private:    int rowCount;
};

//////////////////////////////////////////////////////////////////////////////
// ListView::NullModel
//////////////////////////////////////////////////////////////////////////////

class ListView::NullModel : public ListView::Model
{
public:     virtual int RowCount()
            {
                return 0;
            }

public:     virtual uistring 
            CellValueToString(int rowIndex, ListView::ColumnRef column)
            {
                return uistring();
            }
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
