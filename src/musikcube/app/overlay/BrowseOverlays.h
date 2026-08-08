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
 * @file BrowseOverlays.h
 * @brief Factory for overlay dialogs used by the browse and directory
 *        layouts.
 * @details Provides static helpers that show the category chooser, the
 *          directory chooser and the indexer progress overlay.
 */

#include <musikcore/library/ILibrary.h>
#include <functional>

namespace musik {
    namespace cube {
        /**
         * @brief Factory for overlay dialogs used by the browse and directory
         *        layouts.
         * @details Provides static helpers that show the category chooser, the
         *          directory chooser and the indexer progress overlay.
         */
        class BrowseOverlays {
            public:
                /**
                 * @brief Shows a dialog that lets the user choose a library
                 *        category to navigate to.
                 * @param library the library that owns the categories
                 * @param callback invoked with the selected category type and
                 *        name when the user confirms
                 */
                static void ShowCategoryChooser(
                    musik::core::ILibraryPtr library,
                    std::function<void(std::string, std::string)> callback);

                /**
                 * @brief Shows a dialog that lets the user choose a local
                 *        directory to browse.
                 * @param library the library used to index the directory
                 * @param callback invoked with the selected directory path
                 */
                static void ShowDirectoryChooser(
                    musik::core::ILibraryPtr library,
                    std::function<void(std::string)> callback);

                /**
                 * @brief Shows the indexer progress overlay for the given
                 *        library.
                 * @param library the library whose indexer progress is shown
                 */
                static void ShowIndexer(musik::core::ILibraryPtr library);
        };
    }
}
