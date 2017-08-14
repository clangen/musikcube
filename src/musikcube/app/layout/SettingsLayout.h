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

#include <cursespp/App.h>
#include <cursespp/Checkbox.h>
#include <cursespp/LayoutBase.h>
#include <cursespp/ListWindow.h>
#include <cursespp/ShortcutsWindow.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/TextLabel.h>
#include <cursespp/TextInput.h>
#include <cursespp/DialogOverlay.h>

#include <app/window/TrackListView.h>
#include <core/audio/PlaybackService.h>
#include <app/model/DirectoryAdapter.h>

#include <glue/audio/MasterTransport.h>

#include <core/library/ILibrary.h>
#include <core/support/Preferences.h>

#include <sigslot/sigslot.h>

#include "ITopLevelLayout.h"

namespace musik {
    namespace cube {
        class SettingsLayout :
            public ITopLevelLayout,
            public cursespp::LayoutBase,
#if (__clang_major__ == 7 && __clang_minor__ == 3)
            public std::enable_shared_from_this<SettingsLayout>,
#endif
            public sigslot::has_slots<>
        {
            public:
                SettingsLayout(
                    cursespp::App& app,
                    musik::core::ILibraryPtr library,
                    musik::core::sdk::IPlaybackService& playback,
                    musik::glue::audio::MasterTransport& transport);

                virtual ~SettingsLayout();

                virtual void OnVisibilityChanged(bool visible);
                virtual bool KeyPress(const std::string& key);

                virtual void SetShortcutsWindow(
                    cursespp::ShortcutsWindow* w);

            protected:
                virtual void OnLayout();

            private:
                void InitializeWindows();
                void RefreshAddedPaths();
                void LoadPreferences();
                void AddSelectedDirectory();
                void RemoveSelectedDirectory();
                void DrillIntoSelectedDirectory();
                void CheckShowFirstRunDialog();
                void UpdateServerAvailability();

                void OnCheckboxChanged(
                    cursespp::Checkbox* checkbox, bool checked);

                void OnOutputDriverDropdownActivated(cursespp::TextLabel* label);
                void OnOutputDeviceDropdownActivated(cursespp::TextLabel* label);
                void OnTransportDropdownActivate(cursespp::TextLabel* label);
                void OnPluginsDropdownActivate(cursespp::TextLabel* label);
                void OnHotkeyDropdownActivate(cursespp::TextLabel* label);
                void OnThemeDropdownActivate(cursespp::TextLabel* label);
                void OnLocaleDropdownActivate(cursespp::TextLabel* label);
                void OnServerDropdownActivate(cursespp::TextLabel* label);
                void OnUpdateDropdownActivate(cursespp::TextLabel* label);

                int64_t ListItemDecorator(
                    cursespp::ScrollableWindow* w,
                    size_t index,
                    size_t line,
                    cursespp::IScrollAdapter::EntryPtr entry);

                cursespp::App& app;
                musik::core::ILibraryPtr library;
                musik::core::IIndexer* indexer;
                musik::core::sdk::IPlaybackService& playback;
                musik::glue::audio::MasterTransport& transport;

                std::shared_ptr<musik::core::Preferences> prefs;

                std::shared_ptr<cursespp::TextLabel> localeDropdown;
                std::shared_ptr<cursespp::TextLabel> outputDriverDropdown;
                std::shared_ptr<cursespp::TextLabel> outputDeviceDropdown;
                std::shared_ptr<cursespp::TextLabel> transportDropdown;
                std::shared_ptr<cursespp::TextLabel> pluginsDropdown;
                std::shared_ptr<cursespp::TextLabel> hotkeyDropdown;
                std::shared_ptr<cursespp::TextLabel> serverDropdown;
                std::shared_ptr<cursespp::TextLabel> updateDropdown;

                std::shared_ptr<cursespp::TextLabel> themeDropdown;
                std::shared_ptr<cursespp::Checkbox> paletteCheckbox;

                std::shared_ptr<cursespp::Checkbox> dotfileCheckbox;
                std::shared_ptr<cursespp::Checkbox> syncOnStartupCheckbox;
                std::shared_ptr<cursespp::Checkbox> removeCheckbox;
                std::shared_ptr<cursespp::Checkbox> seekScrubCheckbox;
                std::shared_ptr<cursespp::Checkbox> minimizeToTrayCheckbox;
                std::shared_ptr<cursespp::Checkbox> startMinimizedCheckbox;
                std::shared_ptr<cursespp::Checkbox> autoUpdateCheckbox;

                std::shared_ptr<cursespp::TextLabel> browseLabel;
                std::shared_ptr<cursespp::TextLabel> addedPathsLabel;
                std::shared_ptr<cursespp::ListWindow> browseList;
                std::shared_ptr<cursespp::ListWindow> addedPathsList;

                std::shared_ptr<cursespp::DialogOverlay> firstRunDialog;

                std::shared_ptr<cursespp::SimpleScrollAdapter> addedPathsAdapter;
                std::shared_ptr<DirectoryAdapter> browseAdapter;

                bool serverAvailable = false;
        };
    }
}
