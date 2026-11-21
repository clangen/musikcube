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

#include <app/window/TrackListView.h>
#include <app/util/AsyncCallbackScope.h>

namespace musik { namespace cube {
    class PlayQueueOverlays {
        public:
            using AddTrackToPlaylistCallback = std::function<void(int64_t)>;
            using AddCategoryToPlaylistCallback = std::function<void(const std::string&, const std::string&, int64_t)>;
            using PlaylistPickedCallback = std::function<void(const musik::core::api::PlaylistEntry&)>;

            static void ShowAddTrackOverlay(
                musik::core::runtime::IMessageQueue& messageQueue,
                musik::core::api::Context& musikcore,
                AsyncCallbackScope::Token token,
                musik::core::api::TrackRef track,
                bool allowPlayNow,
                AddTrackToPlaylistCallback addToPlaylist = {});

            static void ShowAddCategoryOverlay(
                musik::core::api::Context& musikcore,
                AsyncCallbackScope::Token token,
                const std::string& fieldColumn,
                const std::string& fieldValue,
                int64_t fieldId,
                AddCategoryToPlaylistCallback addToPlaylist = {});

            static void ShowAlbumDividerOverlay(
                musik::core::runtime::IMessageQueue& messageQueue,
                musik::core::api::Context& musikcore,
                AsyncCallbackScope::Token token,
                musik::core::api::TrackRef firstTrack,
                AddCategoryToPlaylistCallback addToPlaylist = {});

            static void ShowPlaylistPicker(musik::core::api::Context& musikcore,
                const std::string& title, bool allowCreate,
                AsyncCallbackScope::Token token, PlaylistPickedCallback picked, int64_t preferredId = 0);
            static void ShowSavePlaylistOverlay(musik::core::api::Context& musikcore,
                AsyncCallbackScope::Token token, const musik::core::api::PlaylistEntry& playlist,
                std::vector<int64_t> tracks, std::function<void(int64_t)> saved = {});
            static void ShowRenamePlaylistOverlay(musik::core::api::Context& musikcore,
                AsyncCallbackScope::Token token, const musik::core::api::PlaylistEntry& playlist,
                std::function<void(int64_t)> renamed = {});
            static void ShowDeletePlaylistOverlay(musik::core::api::Context& musikcore,
                AsyncCallbackScope::Token token, const musik::core::api::PlaylistEntry& playlist,
                std::function<void(int64_t)> deleted = {});

            static void ShowAddToPlaylistPicker(musik::core::api::Context& musikcore,
                AsyncCallbackScope::Token token,
                std::vector<int64_t> ids);
            static void ShowAddCategoryToPlaylistPicker(musik::core::api::Context& musikcore,
                AsyncCallbackScope::Token token,
                const std::string& field, const std::string& value, int64_t id);

        private:
            PlayQueueOverlays() noexcept;

            static void ShowPlaylistNameOverlay(const std::string& title, const std::string& name,
                std::function<void(const std::string&)> accepted);
            static void ShowPlaylistConfirmation(const std::string& messageKey, const std::string& name,
                std::function<void()> accepted);
    };
} }
