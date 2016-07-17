//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <cursespp/Checkbox.h>
#include <cursespp/LayoutBase.h>
#include <cursespp/ListWindow.h>
#include <cursespp/TextLabel.h>
#include <cursespp/TextInput.h>
#include <cursespp/SimpleScrollAdapter.h>

#include <app/window/TrackListView.h>
#include <app/window/ShortcutsWindow.h>
#include <app/service/PlaybackService.h>
#include <app/model/DirectoryAdapter.h>

#include <core/library/ILibrary.h>
#include <core/support/Preferences.h>

#include <sigslot/sigslot.h>

#include "ITopLevelLayout.h"

#define GENERAL_PREFS_FOCUS_SHORTCUTS "EscFocusesShortcuts"

namespace musik {
    namespace box {
        class SettingsLayout :
            public ITopLevelLayout,
            public cursespp::LayoutBase,
#if (__clang_major__ == 7 && __clang_minor__ == 3)
            public std::enable_shared_from_this<SettingsLayout>,
#endif
            public sigslot::has_slots<>
        {
            public:
                SettingsLayout(musik::core::LibraryPtr library);

                virtual ~SettingsLayout();

                virtual void Layout();
                virtual void OnVisibilityChanged(bool visible);
                virtual bool KeyPress(const std::string& key);

                virtual void SetShortcutsWindow(ShortcutsWindow* w);

            private:
                void InitializeWindows();
                void RefreshAddedPaths();
                void LoadPreferences();
                void AddSelectedDirectory();
                void RemoveSelectedDirectory();
                void DrillIntoSelectedDirectory();

                void OnRemoveMissingCheckChanged(
                    cursespp::Checkbox* checkbox, bool checked);

                void OnDotfilesCheckChanged(
                    cursespp::Checkbox* checkbox, bool checked);

                void OnFocusShortcutsCheckChanged(
                    cursespp::Checkbox* checkbox, bool checked);

                int64 ListItemDecorator(
                    cursespp::ScrollableWindow* w,
                    size_t index,
                    size_t line,
                    cursespp::IScrollAdapter::EntryPtr entry);

                musik::core::LibraryPtr library;
                musik::core::IIndexer* indexer;

                std::shared_ptr<musik::core::Preferences> prefs;

                std::shared_ptr<cursespp::Checkbox> removeCheckbox;
                std::shared_ptr<cursespp::Checkbox> dotfileCheckbox;
                std::shared_ptr<cursespp::Checkbox> focusShortcuts;

                std::shared_ptr<cursespp::TextLabel> browseLabel;
                std::shared_ptr<cursespp::TextLabel> addedPathsLabel;
                std::shared_ptr<cursespp::ListWindow> browseList;
                std::shared_ptr<cursespp::ListWindow> addedPathsList;

                std::shared_ptr<cursespp::TextLabel> hotkeyLabel;
                std::shared_ptr<cursespp::TextInput> hotkeyInput;

                cursespp::SimpleScrollAdapter addedPathsAdapter;
                DirectoryAdapter browseAdapter;
        };
    }
}
