//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <cursespp/LayoutBase.h>
#include <cursespp/TextLabel.h>

#include <app/window/CategoryListView.h>
#include <app/window/TrackListView.h>
#include <core/audio/PlaybackService.h>
#include <core/support/Preferences.h>
#include <core/library/ILibrary.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace cube {
        class BrowseLayout :
            public cursespp::LayoutBase,
            public sigslot::has_slots<>
        {
            public:
                BrowseLayout(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library);

                virtual ~BrowseLayout();

                virtual void OnVisibilityChanged(bool visible) override;
                virtual bool KeyPress(const std::string& key) override;
                virtual void ProcessMessage(musik::core::runtime::IMessage &message) override;

                void ScrollTo(const std::string& fieldType, int64_t fieldId);
                void SwitchCategory(const std::string& fieldName);

                void LoadLastSession();

            protected:
                virtual void OnLayout() override;

            private:
                void InitializeWindows();
                void SaveSession();

                void OnIndexerProgress(int count);
                void RequeryTrackList(cursespp::ListWindow *view);

                void OnCategoryViewSelectionChanged(
                    cursespp::ListWindow *view, size_t newIndex, size_t oldIndex);

                void OnCategoryViewInvalidated(
                    cursespp::ListWindow *view, size_t selectedIndex);

                void OnRequeried(musik::core::db::local::TrackListQueryBase* query);

                bool IsPlaylist();
                bool ProcessEditOperation(const std::string& key);
                bool ProcessPlaylistOperation(const std::string& key);
                void ShowModifiedLabel(bool show);

                bool playlistModified;
                musik::core::audio::PlaybackService& playback;
                musik::core::ILibraryPtr library;
                std::shared_ptr<musik::core::Preferences> prefs;
                std::shared_ptr<CategoryListView> categoryList;
                std::shared_ptr<TrackListView> trackList;
                std::shared_ptr<cursespp::TextLabel> modifiedLabel;
        };
    }
}
