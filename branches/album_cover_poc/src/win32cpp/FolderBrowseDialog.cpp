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

#include "pch.hpp"
#include <win32cpp/FolderBrowseDialog.hpp>

#include <shlobj.h>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

typedef FolderBrowseDialog::Result Result;

//////////////////////////////////////////////////////////////////////////////

static int CALLBACK FolderBrowseSetInitialDirectory(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    if ((uMsg == BFFM_INITIALIZED) && (lpData))
    {
        ::SendMessage(hWnd, BFFM_SETSELECTION, TRUE, lpData);
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    FolderBrowseDialog::FolderBrowseDialog()
{
}

Result      FolderBrowseDialog::Show(Window* owner, const uichar* initialPath)
{
    TCHAR path[_MAX_PATH];

    BROWSEINFO browseInfo;
    ::SecureZeroMemory(&browseInfo, sizeof(browseInfo));
    //
    browseInfo.lpszTitle = _T("Select a directory to continue.");
    browseInfo.hwndOwner = owner ? owner->Handle() : NULL;
    browseInfo.ulFlags |= BIF_NEWDIALOGSTYLE;
    browseInfo.pidlRoot = NULL;

    if (initialPath != NULL)
    {
        browseInfo.lpfn = FolderBrowseSetInitialDirectory;
        browseInfo.lParam = (LPARAM)(LPTSTR)initialPath;
    }

    LPITEMIDLIST pidl = SHBrowseForFolder(&browseInfo);

    if (pidl != NULL)
    {
        SHGetPathFromIDList(pidl, path);

        IMalloc* imalloc = NULL;
        //
        if (SUCCEEDED(SHGetMalloc(&imalloc)))
        {
            imalloc->Free(pidl);
            imalloc->Release();
        }

        this->directory = path;

        if (this->directory.size() > 0)
        {
            if (this->directory[directory.size() - 1] != '\\')
            {
                this->directory += '\\';
            }
        }

        return ResultOK;
    }

    return ResultCanceled;
}

uistring    FolderBrowseDialog::Directory()
{
    return this->directory;
}
