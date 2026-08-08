//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2008, Casey Langen, Andr� W�sten
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

/**
 * @file ComboBox.hpp
 * @brief Combo box control backed by a data Model.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. ComboBox wraps the
 * Win32 combobox control (CBS_DROPDOWNLIST or CBS_SIMPLE). Like ListView,
 * it uses a ComboBox::Model to supply items on demand and emits the
 * SelectionChangedEvent signal when the user picks a different entry.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Window.hpp>
#include <win32cpp/Win32Exception.hpp>
#include <win32cpp/Color.hpp>
#include <win32cpp/ImageList.hpp>

#include <boost/format.hpp>

#include <vector>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// ComboBox
//////////////////////////////////////////////////////////////////////////////

/** @brief A combo box whose items come from a ComboBox::Model.
 *  @details Wraps the Win32 combobox control. The Model supplies the item
 *           count and per-item strings, optional images and indentation.
 *           When the selection changes, the SelectionChangedEvent signal
 *           is emitted.
 *  @see ComboBox::Model */
class ComboBox : public Window
{
public:
    /** @brief The presentation style of the combo box. */
    enum DisplayType {
        DisplayType_Simple          = CBS_SIMPLE,      /*!< list and edit always visible */
        DisplayType_DropDownList    = CBS_DROPDOWNLIST /*!< drop-down list */
    };

    static const LPCWSTR BoxType_Standard;  /**< standard box type string */
    static const LPCWSTR BoxType_Extended;  /**< extended box type string */

    class Model;

    typedef std::shared_ptr<Model>    ModelRef;        /**< shared pointer to a Model */
    typedef sigslot::signal1<ComboBox*> SelectionChangedEvent; /**< signal emitted on selection change */

    /** @brief Constructs a combo box.
     *  @param displayType the presentation style
     *  @param boxType the Win32 box type string */
    ComboBox(
        DisplayType displayType = DisplayType_DropDownList,
        LPCWSTR boxType = BoxType_Extended
    );
    /** @brief Destroys the combo box. */
    ~ComboBox();

    /** @brief Sets the model that supplies the items.
     *  @param model the model to use */
    void                SetModel(ModelRef model);
    /** @brief Returns the index of the selected item.
     *  @return the selected index */
    int                 Selected();
    /** @brief Selects the item at the given index.
     *  @param index the index to select */
    void                Select(int index);
    /** @brief Retrieves combo box information.
     *  @param pcbi the COMBOBOXINFO structure to fill
     *  @return true on success */
    bool                Info(PCOMBOBOXINFO pcbi);

protected:
    ModelRef            model;      /**< the model backing the combo box */
    static ModelRef     sNullModel; /**< shared empty model */

    /** @brief Creates the underlying HWND. */
    virtual HWND        Create(Window* parent);
    /** @brief Refreshes the control when the model's data changes. */
    virtual void        OnDataChanged();

private:
    typedef Window base;
    class NullModel;
    DisplayType displayType; /**< presentation style */
    LPCWSTR boxType;         /**< Win32 box type string */
};

//////////////////////////////////////////////////////////////////////////////
// ComboBox::Model
//////////////////////////////////////////////////////////////////////////////

/** @brief Supplies items to a ComboBox.
 *  @details Subclass and override ItemToString() to provide item text;
 *           ItemToImageListIndex(), ItemToIndent() and ItemToExtendedData()
 *           can be overridden to supply optional per-item attributes. */
class ComboBox::Model
{
private:
    int itemCount; /**< number of items in the model */
public:
    /** @brief Signal emitted when the model's data changes. */
    typedef sigslot::signal0<> DataChangedEvent;

    DataChangedEvent DataChanged; /**< emitted when items change */

    /** @brief Constructs a model with the given item count.
     *  @param itemCount the initial number of items */
    Model(int itemCount = 0) :
      itemCount(itemCount)
    {
    }

    /** @brief Returns the number of items.
     *  @return the item count */
    virtual int ItemCount()
    {
        return this->itemCount;
    }

    /** @brief Returns the image list used by the combo box.
     *  @return an ImageList, or NULL if no images are used */
    virtual ImageList*  ImageList()
    {
        return NULL;
    }

    /** @brief Maps an item index to an image list index.
     *  @param index the item index
     *  @return the image index, or -1 for no image */
    virtual int ItemToImageListIndex(int index)
    {
        return -1;
    }

    /** @brief Returns the string to display for the given item.
     *  @param index the item index
     *  @return the item's text */
    virtual uistring    ItemToString(int index) = 0;
    /** @brief Returns the indentation level of the given item.
     *  @param index the item index
     *  @return the indent level */
    virtual int         ItemToIndent(int index) = 0;
    /** @brief Returns the extended data of the given item.
     *  @param index the item index
     *  @return the item's extended (LPARAM) data */
    virtual LPARAM      ItemToExtendedData(int index) = 0;
};

//////////////////////////////////////////////////////////////////////////////
// ComboBox::NullModel
//////////////////////////////////////////////////////////////////////////////

/** @brief Empty model used when no ComboBox::Model is assigned. */
class ComboBox::NullModel : public ComboBox::Model
{
public:
    virtual int ItemCount()
    {
        return 0;
    }

    virtual uistring ItemToString(int index)
    {
        return uistring();
    }

    virtual int ItemToImageListIndex(int index)
    {
        return -1;
    }

    virtual int ItemToIndent(int index)
    {
        return 0;
    }

    virtual LPARAM ItemToExtendedData(int index)
    {
        return 0;
    }
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp