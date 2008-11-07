//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Casey Langen, André Wösten
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

#include <win32cpp/ComboBox.hpp>
#include <cube/dialog/PreferencesCategory.hpp>
#include <vector>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

namespace musik { namespace cube { namespace dialog {

//////////////////////////////////////////////////////////////////////////////
// PreferencesCategoriesModel
//////////////////////////////////////////////////////////////////////////////
class PreferencesCategoriesModel : public ComboBox::Model
{
private:
    typedef std::vector<PreferencesCategory> categoryTreeVector;
    categoryTreeVector categoryTree;
public:
    enum Categories {
        Libraries                               = 0x1000,
        ClientServer                            = 0x2000,
        AudioSettings                           = 0x3000,     
        AudioSettings_Output,
        Display                                 = 0x4000,
        Display_Ordering,
        Display_TraySettings,
        Display_TraySettings_ShowTrayIcon,
        Display_TraySettings_MinimizeToTray
    };

    PreferencesCategoriesModel();

    virtual int         ItemCount();
    virtual int         ItemToIndent(int index);
    virtual uistring    ItemToString(int index);    
    virtual LPARAM      ItemToExtendedData(int index);
};

//////////////////////////////////////////////////////////////////////////////

} } }    // musik::cube::dialog
