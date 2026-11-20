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

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/library/query/TrackListQueryBase.h>
#include <app/window/TrackListView.h>
#include <app/util/RequestGeneration.h>

namespace musik {
    namespace cube {
        class PlayQueueOverlays {
            public:
                using QueryCallback = musik::core::ILibrary::Callback;

                static void ShowAddTrackOverlay(
                    musik::core::runtime::IMessageQueue& messageQueue,
                    musik::core::ILibraryPtr library,
                    musik::core::audio::PlaybackService& playback,
                    musik::core::TrackPtr track,
                    std::shared_ptr<const musik::core::api::TrackRef> trackRef = {});

                static void ShowAddCategoryOverlay(
                    musik::core::runtime::IMessageQueue& messageQueue,
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    const std::string& fieldColumn,
                    const std::string& fieldValue,
                    int64_t fieldId);

                static void ShowAddDirectoryOverlay(
                    musik::core::runtime::IMessageQueue& messageQueue,
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    const std::string& directory);

                static void ShowAlbumDividerOverlay(
                    musik::core::runtime::IMessageQueue& messageQueue,
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    musik::core::TrackPtr firstTrack);

                using PlaylistPickedCallback = std::function<void(const musik::core::api::PlaylistEntry&)>;
                static void ShowPlaylistPicker(musik::core::api::Context& context,
                    const musik::core::api::Library& library, const std::string& title, bool allowCreate,
                    RequestGeneration::Ticket ticket, PlaylistPickedCallback picked);
                static void ShowPlaylistNameOverlay(const std::string& title, const std::string& name,
                    std::function<void(const std::string&)> accepted);
                static void ShowPlaylistConfirmation(const std::string& messageKey, const std::string& name,
                    std::function<void()> accepted);

                static void ShowRenamePlaylistOverlay(
                    musik::core::ILibraryPtr library,
                    const int64_t playlistId,
                    const std::string& old,
                    QueryCallback callback = QueryCallback());

                static void ShowConfirmDeletePlaylistOverlay(
                    musik::core::ILibraryPtr library,
                    const std::string& playlistName,
                    const int64_t playlistId,
                    QueryCallback callback = QueryCallback());

                static void ShowCreatePlaylistOverlay(
                    musik::core::runtime::IMessageQueue& queue,
                    musik::core::ILibraryPtr library,
                    QueryCallback callback = QueryCallback());

            private:
                PlayQueueOverlays() noexcept;
        };
    }
}
