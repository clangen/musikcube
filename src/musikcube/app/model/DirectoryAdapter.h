//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

/**
 * @file DirectoryAdapter.h
 * @brief Scroll adapter that models a directory tree for a ListWindow.
 * @details Walks the file system from a root directory, listing subfolders
 *          (optionally dot files) with parent and current-directory header
 *          entries, and keeps a stack of previously selected indexes for
 *          drilling into and out of folders.
 */

#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/ListWindow.h>

#include <filesystem>
#include <vector>
#include <stack>

namespace musik {
    namespace cube {
        /**
         * @brief Scroll adapter for browsing the file system by directory.
         * @details Exposes the subdirectories of the current folder to a
         *          ListWindow, rendering a parent entry and a current
         *          directory header when appropriate. Supports entering and
         *          escaping directories, filtering dot files, and returning
         *          paths for the selected entries.
         */
        class DirectoryAdapter : public cursespp::ScrollAdapterBase {
            public:
                /** @brief sentinel index used when no entry is selected */
                static const size_t NO_INDEX = (size_t)-1;

                /**
                 * @brief Creates an empty adapter.
                 */
                DirectoryAdapter();
                /**
                 * @brief Destroys the adapter.
                 */
                virtual ~DirectoryAdapter();

                /**
                 * @brief Selects the entry at the current selection in the
                 *        given list window, drilling into or out of folders.
                 * @param window the list window with the current selection
                 * @return the new selection index
                 */
                size_t Select(cursespp::ListWindow* window);
                /**
                 * @brief Returns the path of the parent of the current folder.
                 * @return the parent path, or an empty string if none
                 */
                std::string GetParentPath();
                /**
                 * @brief Returns the currently browsed directory.
                 * @return the current directory path
                 */
                std::string GetCurrentPath();
                /**
                 * @brief Returns the full path of the entry at the given index.
                 * @param index the adapter index
                 * @return the full path of the entry
                 */
                std::string GetFullPathAt(size_t index);
                /**
                 * @brief Returns the leaf (file name) of the entry at the given index.
                 * @param index the adapter index
                 * @return the leaf name
                 */
                std::string GetLeafAt(size_t index);
                /**
                 * @brief Returns true if the entry at the given index has subdirectories.
                 * @param index the adapter index
                 * @return true if the entry is a non-empty directory
                 */
                bool HasSubDirectories(size_t index);
                /**
                 * @brief Returns true if the current directory has subdirectories.
                 * @return true if the current directory has subdirectories
                 */
                bool HasSubDirectories();
                /**
                 * @brief Sets the root directory of the browser.
                 * @param fullPath the absolute path of the root directory
                 */
                void SetRootDirectory(const std::string& fullPath);
                /**
                 * @brief Controls whether navigation may escape the root directory.
                 * @param allowEscape true to allow navigating above the root
                 */
                void SetAllowEscapeRoot(bool allowEscape);
                /**
                 * @brief Returns the index of the entry with the given leaf name.
                 * @param leaf the leaf name to find
                 * @return the matching index, or NO_INDEX
                 */
                size_t IndexOf(const std::string& leaf);
                /**
                 * @brief Controls whether dot files are listed.
                 * @param visible true to show dot files
                 */
                void SetDotfilesVisible(bool visible);
                /**
                 * @brief Controls whether the root directory header is shown.
                 * @param showRootDirectory true to show the root directory header
                 */
                void SetShowRootDirectory(bool showRootDirectory);
                /**
                 * @brief Returns true if the browser is at the root directory.
                 * @return true if at the root
                 */
                bool IsAtRoot();
                /**
                 * @brief Re-reads the current directory from the file system.
                 */
                void Refresh();

                /* ScrollAdapterBase */
                /**
                 * @brief Returns the number of entries currently listed.
                 * @return the entry count
                 */
                size_t GetEntryCount() override;
                /**
                 * @brief Returns the entry at the given index for rendering.
                 * @param window the scrollable window requesting the entry
                 * @param index the entry index
                 * @return the entry, or an empty entry
                 */
                EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) override;

            private:
                bool ShowParentPath();
                bool ShowCurrentDirectory();
                bool IsCurrentDirectory(size_t index);
                size_t GetHeaderCount();

                std::filesystem::path dir, rootDir;      /**< the current and root directories */
                std::vector<std::string> subdirs;        /**< the subdirectories of the current folder */
                std::stack<size_t> selectedIndexStack;   /**< stack of selected indexes for back-navigation */
                bool showDotfiles, allowEscapeRoot, showRootDirectory; /**< display/navigation options */
        };
    }
}
