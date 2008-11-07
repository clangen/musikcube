//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, mC2 Team
//
// Sources and Binaries of: mC2
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
#include <win32cpp/ApplicationThread.hpp>
#include <cube/dialog/PreferencesCategoriesModel.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube::dialog;

//////////////////////////////////////////////////////////////////////////////

PreferencesCategoriesModel::PreferencesCategoriesModel()
{
    // Fill category tree with data
    PreferencesCategory items[] = { 
    /*  title                                   level  identifier (for inserts etc.) */
        {_(_T("Libraries")),                    0,     Categories::Libraries},
        {_(_T("Client/Server")),                0,     Categories::ClientServer},
        {_(_T("Audio Settings")),               0,     Categories::AudioSettings},
            {_(_T("Output")),                   1,     Categories::AudioSettings_Output},
        {_(_T("Display")),                      0,     Categories::Display},
            {_(_T("Ordering")),                 1,     Categories::Display_Ordering},
            {_(_T("Tray settings")),            1,     Categories::Display_TraySettings},
                {_(_T("Show tray icon")),       2,     Categories::Display_TraySettings_ShowTrayIcon},
                {_(_T("Minimize to tray")),     2,     Categories::Display_TraySettings_MinimizeToTray}
    };

    for(int i = 0; i < sizeof(items) / sizeof(PreferencesCategory); i++) {
        categoryTree.push_back(items[i]);    
    }

}

int PreferencesCategoriesModel::ItemCount()
{
    return (int)categoryTree.size();
}

int PreferencesCategoriesModel::ItemToIndent(int index)
{
    if(index >= 0 && index < categoryTree.size()) {
        return categoryTree[index].indent;
    }

    return 0;
}

uistring PreferencesCategoriesModel::ItemToString(int index)
{
    if(index >= 0 && index < categoryTree.size()) {
        return categoryTree[index].title;
    }

    return uistring();
}

LPARAM PreferencesCategoriesModel::ItemToExtendedData(int index)
{
    if(index >= 0 && index < categoryTree.size()) {
        return categoryTree[index].data;
    }

    return 0;
}