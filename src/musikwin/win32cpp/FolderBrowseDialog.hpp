//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2007, Casey Langen
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
 * @file FolderBrowseDialog.hpp
 * @brief Modal folder selection dialog.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. FolderBrowseDialog
 * wraps the Win32 SHBrowseForFolder API to prompt the user for a folder.
 * The chosen path is available through Directory() after a successful Show().
 */

#pragma once

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Window.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief A folder selection dialog.
 *  @details Wraps the Win32 SHBrowseForFolder common dialog. Show() blocks
 *           until the dialog is closed; on success the selected directory
 *           is available through Directory(). */
class FolderBrowseDialog
{
public: // types
    /** @brief The result of showing the dialog. */
    enum Result
    {
        ResultOK = 0,       /*!< the user picked a folder */
        ResultCanceled      /*!< the user cancelled the dialog */
    };

public: // ctor
    /** @brief Constructs the folder browse dialog. */
    FolderBrowseDialog();

public: // methods
    /** @brief Shows the modal dialog.
     *  @param owner the owner window (may be NULL)
     *  @param initialPath the initially selected folder (may be NULL)
     *  @return ResultOK if the user selected a folder */
    Result Show(Window* owner = NULL, const uichar* initialPath = NULL);
    /** @brief Returns the selected directory.
     *  @return the path chosen by the user */
    uistring Directory();

private: // instance data
    uistring directory; /**< the last selected directory */
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
