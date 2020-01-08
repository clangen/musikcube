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

#include <cursespp/Colors.h>
#include <cursespp/LayoutBase.h>
#include <cursespp/TextLabel.h>

#include <app/window/CategoryListView.h>
#include <app/window/TrackListView.h>
#include <app/model/DirectoryAdapter.h>
#include <core/audio/PlaybackService.h>

#include <core/library/ILibrary.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace cube {
        class DirectoryLayout :
            public cursespp::LayoutBase,
            public sigslot::has_slots<>
        {
            public:
                DirectoryLayout(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library);

                virtual ~DirectoryLayout();

                virtual void OnVisibilityChanged(bool visible);
                virtual bool KeyPress(const std::string& key);
                virtual void SetDirectory(const std::string& directory);

            protected:
                virtual void OnLayout();

            private:
                void InitializeWindows();
                void Refresh(bool requery = false);
                void Requery(bool invalidate = false);
                void RequeryTrackList(cursespp::ListWindow *view);
                void UpdateTitle();
                bool IsParentSelected();
                bool IsParentRoot();

                cursespp::Color ListItemDecorator(
                    cursespp::ScrollableWindow* scrollable,
                    size_t index,
                    size_t line,
                    cursespp::IScrollAdapter::EntryPtr entry);

                void OnDirectoryChanged(
                    cursespp::ListWindow *view,
                    size_t newIndex,
                    size_t oldIndex);

                void OnIndexerProgress(int count);

                musik::core::audio::PlaybackService& playback;
                musik::core::ILibraryPtr library;
                std::string rootDirectory;
                std::shared_ptr<DirectoryAdapter> adapter;
                std::shared_ptr<cursespp::ListWindow> directoryList;
                std::shared_ptr<TrackListView> trackList;
                size_t queryHash;
                bool hasSubdirectories;
        };
    }
}
