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
 * @file LocalLibrarySettingsLayout.h
 * @brief Layout for configuring which local folders are indexed.
 * @details Lets the user browse the file system, add or remove indexed
 *          directories and toggle whether dot files are shown.
 */

#include <cursespp/LayoutBase.h>
#include <cursespp/ListWindow.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <musikcore/library/ILibrary.h>
#include <app/model/DirectoryAdapter.h>

namespace musik { namespace cube {
    /**
     * @brief Local library folder settings layout.
     * @details Provides a file browser to drill into directories, lists the
     *          currently indexed paths, and supports adding, removing and
     *          toggling dot file visibility.
     */
    class LocalLibrarySettingsLayout: public cursespp::LayoutBase {
        public:
            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(LocalLibrarySettingsLayout)

            /**
             * @brief Creates the settings layout.
             */
            LocalLibrarySettingsLayout();

            /**
             * @brief Toggles whether dot files are shown in the browser.
             */
            void ToggleShowDotFiles();
            /**
             * @brief Loads the persisted folder and preference settings.
             */
            void LoadPreferences();

            /* IWindow */
            /**
             * @brief Handles keyboard input.
             * @param key the key sequence that was pressed
             * @return true if the event was consumed
             */
            bool KeyPress(const std::string& key) override;
            /**
             * @brief Positions and lays out the child windows.
             */
            void OnLayout() override;

        private:
            void InitializeWindows();
            void AddSelectedDirectory();
            void RemoveSelectedDirectory();
            void DrillIntoSelectedDirectory();

            cursespp::Color ListItemDecorator(
                cursespp::ScrollableWindow* w,
                size_t index,
                size_t line,
                cursespp::IScrollAdapter::EntryPtr entry);

            musik::core::ILibraryPtr library;             /**< the library whose index is configured */
            musik::core::IIndexer* indexer;               /**< the library indexer, not owned */
            std::shared_ptr<cursespp::ListWindow> browseList;      /**< the file browser list */
            std::shared_ptr<cursespp::ListWindow> addedPathsList;  /**< the list of indexed paths */
            std::shared_ptr<cursespp::SimpleScrollAdapter> addedPathsAdapter; /**< the adapter for indexed paths */
            std::shared_ptr<DirectoryAdapter> browseAdapter;       /**< the adapter for the file browser */
    };
} }
