//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Casey Langen, André Wösten
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
#include <win32cpp/ImageList.hpp>

#include <boost/format.hpp>
#include <boost/scoped_ptr.hpp>

#include <vector>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// ComboBox
//////////////////////////////////////////////////////////////////////////////

class ComboBox : public Window
{
public: 
    enum DisplayType {
        DisplayType_Simple          = CBS_SIMPLE,
        DisplayType_DropDownList    = CBS_DROPDOWNLIST
    };

    static const LPCWSTR BoxType_Standard;
    static const LPCWSTR BoxType_Extended;

    class Model;

    typedef boost::shared_ptr<Model>    ModelRef;
    typedef sigslot::signal1<ComboBox*> SelectionChangedEvent;

    ComboBox(
        DisplayType displayType = DisplayType_DropDownList,
        LPCWSTR boxType = BoxType_Extended
    );
    ~ComboBox();

    void                SetModel(ModelRef model);
    int                 Selected();
    void                Select(int index);
    bool                Info(PCOMBOBOXINFO pcbi);

protected: 
    ModelRef            model;
    static ModelRef     sNullModel;

    virtual HWND        Create(Window* parent);    
    virtual void        OnDataChanged();

private: 
    typedef Window base;
    class NullModel;
    DisplayType displayType;
    LPCWSTR boxType;
};

//////////////////////////////////////////////////////////////////////////////
// ComboBox::Model
//////////////////////////////////////////////////////////////////////////////

class ComboBox::Model 
{
private:
    int itemCount;
public:
    typedef sigslot::signal0<> DataChangedEvent;
    
    DataChangedEvent DataChanged;

    Model(int itemCount = 0) : 
      itemCount(itemCount)
    {
    }

    virtual int ItemCount() 
    {
        return this->itemCount;
    }

    virtual ImageList*  ImageList() 
    {
        return NULL;
    }

    virtual int ItemToImageListIndex(int index)
    {
        return -1;
    }

    virtual uistring    ItemToString(int index) = 0;
    virtual int         ItemToIndent(int index) = 0;
    virtual LPARAM      ItemToExtendedData(int index) = 0;
};

//////////////////////////////////////////////////////////////////////////////
// ComboBox::NullModel
//////////////////////////////////////////////////////////////////////////////

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