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

#include <stdafx.h>
#include <musikcore/log.hpp>

#include <musikcore/library.h>

#include <chrono>

#include "PlayQueueOverlays.h"


#include <common/runtime/Message.h>

#include <app/util/Messages.h>
#include <app/util/MagicConstants.h>

#include <cursespp/App.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/InputOverlay.h>

#include <functional>

#define DEFAULT_OVERLAY_WIDTH 30

using namespace musik;
using namespace musik::core;
using namespace musik::core::runtime;
using namespace musik::cube;
using namespace cursespp;

using namespace std::chrono;
using Seconds = std::chrono::duration<float>;

static size_t lastTrackOverlayIndex = 0;
static size_t lastCategoryOverlayIndex = 0;
static int64_t lastPlaylistId = -1;
static milliseconds lastOperationExpiry;

using Adapter = cursespp::SimpleScrollAdapter;

static inline milliseconds now() noexcept {
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch());
}

static inline bool lastOperationExpired() noexcept {
    return now() > lastOperationExpiry;
}

static inline void touchOperationExpiry() noexcept {
    lastOperationExpiry = now() + duration_cast<milliseconds>(Seconds(60));
}

static inline void setLastPlaylistId(int64_t id) {
    lastPlaylistId = id;
    touchOperationExpiry();
}

static inline void setLastTrackOverlayIndex(size_t i) {
    lastTrackOverlayIndex = i;
    touchOperationExpiry();
}

static inline void setLastCategoryOverlayIndex(size_t i) {
    lastCategoryOverlayIndex = i;
    touchOperationExpiry();
}

static std::string stringWithFormat(const std::string& key, const std::string& value) {
    return u8fmt(_TSTR(key), value.c_str());
}

static void showPlaylistListOverlay(
    const std::string& title,
    std::shared_ptr<Adapter> adapter,
    ListOverlay::ItemSelectedCallback callback,
    size_t selectedIndex = 0)
{
    auto dialog = std::make_shared<ListOverlay>();

    dialog->SetAdapter(adapter)
        .SetTitle(title)
        .SetWidth(_DIMEN("playqueue_playlist_list_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(std::move(callback));

    cursespp::App::Overlays().Push(dialog);
}

static void showNoPlaylistsDialog() {
    auto dialog = std::make_shared<DialogOverlay>();

    (*dialog)
        .SetTitle(_TSTR("default_overlay_title"))
        .SetMessage(_TSTR("playqueue_overlay_load_playlists_none_message"))
        .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"));

    App::Overlays().Push(dialog);
}

static void readCategoryId(api::Context& musikcore, const api::TrackRef& track,
    AsyncCallbackScope::Token token, const char* field, std::function<void(int64_t)> resolved)
{
    api::ReadTrack(musikcore, track, { field },
        [token, field, resolved = std::move(resolved)](mc_result status, api::TrackPage page, std::string_view) {
            if (!token.IsCurrent() || status != MC_OK || !page.Size()) {
                return;
            }

            const auto value = page.Track(0).Value(field);
            if (!value.empty()) {
                int64_t id = 0;
                const auto end = value.data() + value.size();
                const auto parsed = std::from_chars(value.data(), end, id);
                if (parsed.ec == std::errc{} && parsed.ptr == end) {
                    try {
                        resolved(id);
                    }
                    catch (const api::Error& error) {
                        musik::core::api::WriteLog(MC_LOG_ERROR, "PlayQueueOverlays", error.what());
                    }
                }
            }
        });
}

static void handleJumpTo(api::Context& musikcore, const api::TrackRef& track,
    AsyncCallbackScope::Token token, size_t index, IMessageQueue& messageQueue)
{
    constexpr const char* fields[] = {
        MC_TRACK_FIELD_ALBUM_ID, MC_TRACK_FIELD_VISUAL_ARTIST_ID, MC_TRACK_FIELD_VISUAL_GENRE_ID
    };
    constexpr int64_t categories[] = {
        cube::message::category::Album, cube::message::category::Artist, cube::message::category::Genre
    };
    readCategoryId(musikcore, track, token, fields[index], [&messageQueue, category = categories[index]](int64_t id) {
        messageQueue.Broadcast(runtime::Message::Create(nullptr, cube::message::JumpToCategory, category, id));
    });
}

static void showPlaylistPicker(const std::string& title, bool allowCreate,
    std::vector<api::PlaylistEntry> playlists, int64_t preferredId,
    PlayQueueOverlays::PlaylistPickedCallback picked)
{
    auto adapter = std::make_shared<Adapter>();
    adapter->SetSelectable(true);
    if (allowCreate) {
        adapter->AddEntry(_TSTR("playqueue_overlay_new"));
    }
    for (const auto& playlist : playlists) {
        adapter->AddEntry(playlist.name);
    }

    const auto preferred = std::find_if(playlists.begin(), playlists.end(),
        [preferredId](const auto& playlist) { return playlist.id == preferredId; });
    const size_t offset = allowCreate ? 1 : 0;
    const size_t selected = preferred == playlists.end() ? 0
        : static_cast<size_t>(std::distance(playlists.begin(), preferred)) + offset;

    showPlaylistListOverlay(title, adapter,
        [offset, picked = std::move(picked), playlists = std::move(playlists)](ListOverlay*, IScrollAdapterPtr, size_t index) {
            if (index < offset) {
                picked({ 0, {} });
            }
            else if (index - offset < playlists.size()) {
                picked(playlists[index - offset]);
            }
        }, selected);
}

PlayQueueOverlays::PlayQueueOverlays() noexcept {
}

void PlayQueueOverlays::ShowAddTrackOverlay(
    IMessageQueue& messageQueue,
    api::Context& musikcore,
    AsyncCallbackScope::Token token,
    api::TrackRef track,
    bool allowPlayNow,
    AddTrackToPlaylistCallback addToPlaylist)
{
    enum TrackActionIndex : size_t {
        JumpToAlbum,
        JumpToArtist,
        JumpToGenre,
        AddToPlaylist,
        Prepend,
        Append,
        PlayNext,
        PlayNow
    };

    auto adapter = std::make_shared<Adapter>();
    adapter->AddEntry(_TSTR("playqueue_overlay_album_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_artist_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_genre_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_playlist"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_start_of_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_end_in_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_as_next_in_queue"));
    if (allowPlayNow) {
        adapter->AddEntry(_TSTR("playqueue_overlay_track_play_now"));
    }
    adapter->SetSelectable(true);

    size_t selectedIndex = 0;
    if (!lastOperationExpired()) {
        selectedIndex = std::min(lastTrackOverlayIndex, size_t(allowPlayNow ? PlayNow : PlayNext));
    }

    auto dialog = std::make_shared<ListOverlay>();

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playqueue_overlay_track_actions_title"))
        .SetWidth(_DIMEN("playqueue_playlist_add_to_queue_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [track = std::make_shared<api::TrackRef>(std::move(track)), token, allowPlayNow,
                addToPlaylist, &messageQueue, &musikcore]
            (ListOverlay*, IScrollAdapterPtr, size_t index) {
                if (!token.IsCurrent() || index > (allowPlayNow ? PlayNow : PlayNext)) {
                    return;
                }
                setLastTrackOverlayIndex(index);

                try {
                    if (index >= Prepend) {
                        constexpr mc_playback_track_action actions[] = {
                            MC_PLAYBACK_TRACK_PREPEND, MC_PLAYBACK_TRACK_APPEND,
                            MC_PLAYBACK_TRACK_PLAY_NEXT, MC_PLAYBACK_TRACK_PLAY_NOW
                        };
                        musikcore.GetPlayback().UseTrack(*track, actions[index - Prepend]);
                    }
                    else if (index == AddToPlaylist) {
                        if (addToPlaylist) {
                            addToPlaylist(track->Id());
                        }
                        else {
                            ShowAddToPlaylistPicker(musikcore, token, { track->Id() });
                        }
                    }
                    else {
                        handleJumpTo(musikcore, *track, token, index, messageQueue);
                    }
                }
                catch (const api::Error& error) {
                    musik::core::api::WriteLog(MC_LOG_ERROR, "PlayQueueOverlays", error.what());
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

static void openCategoryTracks(api::Context& musikcore,
    const std::string& field, const std::string& value, int64_t id, api::TrackSearchCompletion completion)
{
    if (field == MagicConstants::DirectoryCategoryType) {
        api::OpenDirectory(musikcore, value, std::move(completion));
    }
    else if (field == MC_LIBRARY_CATEGORY_PLAYLISTS) {
        api::OpenPlaylist(musikcore, id, std::move(completion));
    }
    else {
        api::OpenCategory(musikcore, field, id, MC_TRACK_SEARCH_ALBUM, std::move(completion));
    }
}

void PlayQueueOverlays::ShowAddCategoryOverlay(
    api::Context& musikcore,
    AsyncCallbackScope::Token token,
    const std::string& fieldColumn,
    const std::string& fieldValue,
    int64_t fieldId,
    AddCategoryToPlaylistCallback addToPlaylist)
{
    auto adapter = std::make_shared<Adapter>();
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_playlist"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_start_of_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_end_in_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_as_next_in_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_hotswap_queue"));
    adapter->SetSelectable(true);

    size_t selectedIndex = 0;
    if (!lastOperationExpired()) {
        selectedIndex = lastCategoryOverlayIndex;
    }

    auto dialog = std::make_shared<ListOverlay>();

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playqueue_overlay_add_category_title"))
        .SetWidth(_DIMEN("playqueue_playlist_add_to_queue_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [&musikcore, token, fieldColumn, fieldValue, fieldId, addToPlaylist]
            (ListOverlay*, IScrollAdapterPtr adapter, size_t index) {

                if (!token.IsCurrent() || index >= adapter->GetEntryCount()) {
                    return;
                }

                setLastCategoryOverlayIndex(index);
                if (index == 0) {
                    if (addToPlaylist) {
                        addToPlaylist(fieldColumn, fieldValue, fieldId);
                    }
                    else {
                        ShowAddCategoryToPlaylistPicker(musikcore, token, fieldColumn, fieldValue, fieldId);
                    }
                    return;
                }

                try {
                    openCategoryTracks(musikcore, fieldColumn, fieldValue, fieldId,
                        [&musikcore, token, index](mc_result status, api::TrackResult result, std::string_view) {

                            if (status != MC_OK || !token.IsCurrent()) {
                                return;
                            }

                            auto handle = musikcore.GetPlayback();
                            constexpr mc_playback_track_action actions[] = {
                                MC_PLAYBACK_TRACK_PREPEND, MC_PLAYBACK_TRACK_APPEND, MC_PLAYBACK_TRACK_PLAY_NEXT
                            };
                            const auto action = index - 1;
                            if (action < std::size(actions)) {
                                handle.InsertResult(result, actions[action]);
                            }
                            else if (result.Size()) {
                                handle.HotSwapQueue(result, 0);
                            }
                        });
                }
                catch (const api::Error& error) {
                    musik::core::api::WriteLog(MC_LOG_ERROR, "PlayQueueOverlays", error.what());
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowAlbumDividerOverlay(
    IMessageQueue& messageQueue, api::Context& musikcore, AsyncCallbackScope::Token token,
    api::TrackRef firstTrack, AddCategoryToPlaylistCallback addToPlaylist)
{
    auto adapter = std::make_shared<Adapter>();
    adapter->AddEntry(_TSTR("playqueue_overlay_album_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_artist_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_genre_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_album_play"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_playlist"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_start_of_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_end_in_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_as_next_in_queue"));
    adapter->SetSelectable(true);

    auto dialog = std::make_shared<ListOverlay>();

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playqueue_overlay_album_header_actions_title"))
        .SetWidth(_DIMEN("playqueue_album_header_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetSelectedIndex(0)
        .SetItemSelectedCallback(
            [&musikcore, token, &messageQueue, firstTrack = std::make_shared<api::TrackRef>(std::move(firstTrack)),
                addToPlaylist](ListOverlay*, IScrollAdapterPtr adapter, size_t index) {
                if (!token.IsCurrent() || index >= adapter->GetEntryCount()) {
                    return;
                }

                constexpr size_t PlayAlbum = 3;
                constexpr size_t AddToPlaylist = 4;
                constexpr size_t FirstQueueInsert = 5;
                try {
                    if (index < PlayAlbum) {
                        handleJumpTo(musikcore, *firstTrack, token, index, messageQueue);
                        return;
                    }

                    readCategoryId(musikcore, *firstTrack, token, MC_TRACK_FIELD_ALBUM_ID,
                        [&musikcore, firstTrack, token, index, addToPlaylist](int64_t albumId) {
                            if (index == AddToPlaylist) {
                                if (addToPlaylist) {
                                    addToPlaylist(MC_TRACK_FIELD_ALBUM, "", albumId);
                                }
                                else {
                                    ShowAddCategoryToPlaylistPicker(musikcore, token, MC_TRACK_FIELD_ALBUM, "", albumId);
                                }
                                return;
                            }

                            api::OpenCategory(musikcore, firstTrack->Origin(), MC_TRACK_FIELD_ALBUM, albumId,
                                MC_TRACK_SEARCH_ALBUM,
                                [&musikcore, token, index](mc_result status, api::TrackResult result, std::string_view) {
                                    if (status != MC_OK || !token.IsCurrent() || !result.Size()) {
                                        return;
                                    }

                                    /* Playback validates the result's origin again before editing the queue. */
                                    auto playback = musikcore.GetPlayback();
                                    if (index == PlayAlbum) {
                                        playback.Play(result, 0);
                                    }
                                    else {
                                        constexpr mc_playback_track_action actions[] = {
                                            MC_PLAYBACK_TRACK_PREPEND, MC_PLAYBACK_TRACK_APPEND, MC_PLAYBACK_TRACK_PLAY_NEXT
                                        };
                                        playback.InsertResult(result, actions[index - FirstQueueInsert]);
                                    }
                                });
                        });
                }
                catch (const api::Error& error) {
                    musik::core::api::WriteLog(MC_LOG_ERROR, "PlayQueueOverlays", error.what());
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowPlaylistPicker(api::Context& musikcore,
    const std::string& title, bool allowCreate, AsyncCallbackScope::Token token, PlaylistPickedCallback picked,
    int64_t preferredId)
{
    api::ListPlaylists(musikcore,
        [title, allowCreate, token, picked = std::move(picked), preferredId]
        (mc_result status, std::vector<api::PlaylistEntry> playlists, std::string_view) mutable {
            if (!token.IsCurrent() || status != MC_OK) {
                return;
            }
            if (!allowCreate && playlists.empty()) {
                showNoPlaylistsDialog();
                return;
            }

            showPlaylistPicker(title, allowCreate, std::move(playlists), preferredId,
                [token, picked = std::move(picked)](const api::PlaylistEntry& playlist) {
                    if (token.IsCurrent()) {
                        picked(playlist);
                    }
                });
        });
}

void PlayQueueOverlays::ShowAddToPlaylistPicker(api::Context& musikcore,
    AsyncCallbackScope::Token token, std::vector<int64_t> ids)
{
    auto selectedLibrary = musikcore.GetLibrary();
    if (!selectedLibrary) {
        return;
    }
    auto target = std::make_shared<api::Library>(std::move(*selectedLibrary));


    if (!token.IsCurrent() || ids.empty()) {
        return;
    }

    auto write = [&musikcore, target, token, tracks = std::move(ids)](int64_t id, const std::string& name) {

        if (!token.IsCurrent()) {
            return;
        }
        const auto complete = [token](mc_result status, int64_t playlistId, std::string_view) {
            if (token.IsCurrent() && status == MC_OK && playlistId > 0) {
                setLastPlaylistId(playlistId);
            }
        };
        try {
            if (id) {
                api::AppendPlaylist(musikcore, *target, id, tracks, complete);
            }
            else {
                api::SavePlaylist(musikcore, *target, 0, name, tracks, complete);
            }
        }
        catch (const api::Error& error) {
            musik::core::api::WriteLog(MC_LOG_ERROR, "PlayQueueOverlays", error.what());
        }
    };

    try {
        ShowPlaylistPicker(musikcore, _TSTR("playqueue_overlay_select_playlist_title"), true, token,
            [write = std::move(write)](const api::PlaylistEntry& playlist) mutable {
                if (playlist.id) {
                    write(playlist.id, {});
                }
                else {
                    ShowPlaylistNameOverlay(_TSTR("playqueue_overlay_playlist_name_title"), {},
                        [write = std::move(write)](const std::string& name) { write(0, name); });
                }
            }, lastOperationExpired() ? 0 : lastPlaylistId);
    }
    catch (const api::Error& error) {
        musik::core::api::WriteLog(MC_LOG_ERROR, "PlayQueueOverlays", error.what());
    }
}

void PlayQueueOverlays::ShowAddCategoryToPlaylistPicker(api::Context& musikcore,
    AsyncCallbackScope::Token token,
    const std::string& field, const std::string& value, int64_t id)
{

    if (!token.IsCurrent()) {
        return;
    }

    const auto selected = [&musikcore, token](mc_result status, api::TrackResult tracks, std::string_view) {
        if (status == MC_OK && token.IsCurrent()) {
            std::vector<int64_t> ids;
            ids.reserve(tracks.Size());
            for (size_t row = 0; row < tracks.Size(); ++row) {
                ids.push_back(tracks.Id(row));
            }
            ShowAddToPlaylistPicker(musikcore, token, std::move(ids));
        }
    };
    try {
        openCategoryTracks(musikcore, field, value, id, selected);
    }
    catch (const api::Error& error) {
        musik::core::api::WriteLog(MC_LOG_ERROR, "PlayQueueOverlays", error.what());
    }
}

static api::PlaylistWriteCompletion playlistCompletion(AsyncCallbackScope::Token token,
    std::function<void(int64_t)> succeeded)
{
    return [token, succeeded = std::move(succeeded)](mc_result status, int64_t id, std::string_view error) {
        if (!token.IsCurrent()) {
            return;
        }
        if (status != MC_OK) {
            musik::core::api::WriteLog(MC_LOG_ERROR, "PlayQueueOverlays", std::string(error));
        }
        else if (succeeded) {
            succeeded(id);
        }
    };
}

void PlayQueueOverlays::ShowSavePlaylistOverlay(api::Context& musikcore, AsyncCallbackScope::Token token,
    const api::PlaylistEntry& playlist, std::vector<int64_t> tracks, std::function<void(int64_t)> saved)
{
    auto selectedLibrary = musikcore.GetLibrary();
    if (!selectedLibrary) {
        return;
    }
    auto target = std::make_shared<api::Library>(std::move(*selectedLibrary));

    /* Own the track order captured when the prompt opens. */
    auto save = [&musikcore, target, token, id = playlist.id, tracks = std::move(tracks),
        completed = playlistCompletion(token, std::move(saved))](const std::string& name) {
        if (token.IsCurrent()) {
            try {
                api::SavePlaylist(musikcore, *target, id, name, tracks, completed);
            }
            catch (const api::Error& error) {
                musik::core::api::WriteLog(MC_LOG_ERROR, "PlayQueueOverlays", error.what());
            }
        }
    };

    if (playlist.id) {
        ShowPlaylistConfirmation("playqueue_overlay_confirm_overwrite_message", playlist.name,
            [save = std::move(save)] { save({}); });
    }
    else {
        ShowPlaylistNameOverlay(_TSTR("playqueue_overlay_new_playlist_name_title"), {}, std::move(save));
    }
}

void PlayQueueOverlays::ShowRenamePlaylistOverlay(api::Context& musikcore, AsyncCallbackScope::Token token,
    const api::PlaylistEntry& playlist, std::function<void(int64_t)> renamed)
{
    auto selectedLibrary = musikcore.GetLibrary();
    if (!selectedLibrary) {
        return;
    }
    auto target = std::make_shared<api::Library>(std::move(*selectedLibrary));

    ShowPlaylistNameOverlay(_TSTR("playqueue_overlay_new_playlist_name_title"), playlist.name,
        [&musikcore, target, token, id = playlist.id, completed = playlistCompletion(token, std::move(renamed))]
        (const std::string& name) {
            if (token.IsCurrent()) {
                try {
                    api::RenamePlaylist(musikcore, *target, id, name, completed);
                }
                catch (const api::Error& error) {
                    musik::core::api::WriteLog(MC_LOG_ERROR, "PlayQueueOverlays", error.what());
                }
            }
        });
}

void PlayQueueOverlays::ShowDeletePlaylistOverlay(api::Context& musikcore, AsyncCallbackScope::Token token,
    const api::PlaylistEntry& playlist, std::function<void(int64_t)> deleted)
{
    auto selectedLibrary = musikcore.GetLibrary();
    if (!selectedLibrary) {
        return;
    }
    auto target = std::make_shared<api::Library>(std::move(*selectedLibrary));

    ShowPlaylistConfirmation("playqueue_overlay_confirm_delete_message", playlist.name,
        [&musikcore, target, token, id = playlist.id, completed = playlistCompletion(token, std::move(deleted))] {
            if (token.IsCurrent()) {
                try {
                    api::DeletePlaylist(musikcore, *target, id, completed);
                }
                catch (const api::Error& error) {
                    musik::core::api::WriteLog(MC_LOG_ERROR, "PlayQueueOverlays", error.what());
                }
            }
        });
}

void PlayQueueOverlays::ShowPlaylistNameOverlay(const std::string& title, const std::string& name,
    std::function<void(const std::string&)> accepted)
{
    auto dialog = std::make_shared<InputOverlay>();
    dialog->SetTitle(title)
        .SetWidth(_DIMEN("playqueue_playlist_name_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetText(name)
        .SetInputAcceptedCallback([accepted = std::move(accepted)](const std::string& value) {
            if (!value.empty()) {
                accepted(value);
            }
        });
    App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowPlaylistConfirmation(const std::string& messageKey, const std::string& name,
    std::function<void()> accepted)
{
    auto dialog = std::make_shared<DialogOverlay>();
    dialog->SetTitle(_TSTR("default_overlay_title"))
        .SetMessage(stringWithFormat(messageKey, name))
        .AddButton("^[", "ESC", _TSTR("button_no"))
        .AddButton("KEY_ENTER", "ENTER", _TSTR("button_yes"),
            [accepted](const std::string&) { accepted(); });
    App::Overlays().Push(dialog);
}
