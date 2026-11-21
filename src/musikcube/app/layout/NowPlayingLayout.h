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

#include <cursespp/Colors.h>
#include <cursespp/LayoutBase.h>
#include <app/window/CategoryListView.h>
#include <app/window/TrackListView.h>
#include <app/util/AsyncCallbackScope.h>


namespace musik { namespace cube {
    class NowPlayingLayout : public cursespp::LayoutBase {
        public:
            DELETE_CLASS_DEFAULTS(NowPlayingLayout)

            NowPlayingLayout(
                musik::core::api::Context& musikcore);

            ~NowPlayingLayout() override;

            void CancelPlaylistLoad();

            /* IWindow */
            void OnVisibilityChanged(bool visible) override;
            bool KeyPress(const std::string& key) override;

        protected:
            /* LayoutBase */
            void OnLayout() override;

        private:
            enum class PlaylistAction {
                Load,
                Save,
                Rename,
                Delete
            };

            void InitializeWindows();
            void RequeryTrackList();
            void SelectQueueRow();
            bool ProcessEditOperation(const std::string& key);

            /* callbacks */
            cursespp::Color RowDecorator(size_t index);
            void OnPlaylistSelected(int64_t playlistId);
            void ShowPlaylistPicker(PlaylistAction action);
            void SaveQueueToPlaylist(const musik::core::api::PlaylistEntry& playlist);
            void RenamePlaylist(const musik::core::api::PlaylistEntry& playlist);
            void DeletePlaylist(const musik::core::api::PlaylistEntry& playlist);

            musik::core::api::Playback apiPlayback;
            std::shared_ptr<TrackListView> trackListView;
            std::optional<musik::core::api::PlayQueue> queue;
            int reselectIndex; /* sigh... */
            musik::core::api::Context& musikcore;
            AsyncCallbackScope playlistWork;
            mc_request_id playlistRequest = 0;
    };
} }
