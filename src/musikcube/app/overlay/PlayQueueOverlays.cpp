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
#include <musikcore/debug.h>
#include <musikcore/ffi/consumer/playback-binding.h>

#include <chrono>

#include "PlayQueueOverlays.h"

#include <musikcore/audio/Visualizer.h>
#include <musikcore/library/LocalLibraryConstants.h>

#include <musikcore/library/query/CategoryTrackListQuery.h>
#include <musikcore/library/query/CategoryListQuery.h>
#include <musikcore/library/query/DirectoryTrackListQuery.h>
#include <musikcore/library/query/GetPlaylistQuery.h>
#include <musikcore/library/query/SavePlaylistQuery.h>
#include <musikcore/library/query/DeletePlaylistQuery.h>
#include <musikcore/runtime/Message.h>

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
using namespace musik::core::audio;
using namespace musik::core::db;
using namespace musik::core::library::query;
using namespace musik::core::library::constants;
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

static void queryTracksByCategory(
    ILibraryPtr library,
    const std::string& categoryType,
    const std::string& categoryValue,
    int64_t categoryId,
    std::function<void(std::shared_ptr<TrackList>)> callback)
{
    /* ShowAddDirectoryOverlay() calls through to ShowAddCategoryOverlay() with a
    special `fieldColumn` called `kDirectoryFieldColumn`. If we detect this case
    we'll run a special query for directory path matching */
    std::shared_ptr<TrackListQueryBase> query;
    if (categoryType == MagicConstants::DirectoryCategoryType) {
        query = std::make_shared<DirectoryTrackListQuery>(library, categoryValue);
    }
    else {
        query = std::make_shared<CategoryTrackListQuery>(library, categoryType, categoryId);
    }

    library->Enqueue(query, [query, callback](auto q) {
        callback(query->GetStatus() == IQuery::Finished
            ? query->GetResult()  : std::shared_ptr<TrackList>());
    });
}

static void queryPlaylists(
    ILibraryPtr library,
    std::function<void(std::shared_ptr<CategoryListQuery>)> callback)
{
    auto query = std::make_shared<CategoryListQuery>(
        CategoryListQuery::MatchType::Substring, Playlists::TABLE_NAME, "");

    auto called = std::make_shared<bool>(false);

    /* jump through a couple extra hooks to try to avoid UI flicker; let's
    wait synchronously for up to 250 milliseconds for the query to return,
    and return results on the calling thread if possible. this will prevent
    ui flicker between overlays. */
    auto completion = [callback, query, called](auto q) {
        if (!*called) {
            callback(query->GetStatus() == IQuery::Finished
                ? query : std::shared_ptr<CategoryListQuery>());
        }
    };

    library->EnqueueAndWait(query, 250, completion);

    if (query->GetStatus() == IQuery::Finished) {
        completion(query);
        *called = true;
    }
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
        .SetItemSelectedCallback(callback);

    cursespp::App::Overlays().Push(dialog);
}

static void createNewPlaylist(
    IMessageQueue& queue,
    std::shared_ptr<TrackList> tracks,
    ILibraryPtr library,
    ILibrary::Callback callback = ILibrary::Callback())
{
    auto dialog = std::make_shared<InputOverlay>();

    dialog->SetTitle(_TSTR("playqueue_overlay_playlist_name_title"))
        .SetWidth(_DIMEN("playqueue_playlist_name_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetText("")
        .SetInputAcceptedCallback(
            [&queue, tracks, library, callback](const std::string& name) {
                if (name.size()) {
                    auto query = SavePlaylistQuery::Save(library, name, tracks);
                    library->Enqueue(query, [&queue, callback](auto query) {
                        setLastPlaylistId(std::static_pointer_cast<SavePlaylistQuery>(query)->GetPlaylistId());
                        if (callback) {
                            callback(query);
                        }
                    });
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowRenamePlaylistOverlay(
    ILibraryPtr library,
    const int64_t playlistId,
    const std::string& oldName,
    QueryCallback callback)
{
    auto dialog = std::make_shared<InputOverlay>();

    dialog->SetTitle(_TSTR("playqueue_overlay_new_playlist_name_title"))
        .SetWidth(_DIMEN("playqueue_playlist_name_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetText(oldName)
        .SetInputAcceptedCallback(
            [library, playlistId, callback](const std::string& name) {
                if (name.size()) {
                    library->Enqueue(SavePlaylistQuery::Rename(library, playlistId, name), callback);
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowConfirmDeletePlaylistOverlay(
    ILibraryPtr library,
    const std::string& playlistName,
    const int64_t playlistId,
    QueryCallback callback)
{
    auto dialog = std::make_shared<DialogOverlay>();

    (*dialog)
        .SetTitle(_TSTR("default_overlay_title"))
        .SetMessage(stringWithFormat(
            "playqueue_overlay_confirm_delete_message",
            playlistName))
        .AddButton("^[", "ESC", _TSTR("button_no"))
        .AddButton(
            "KEY_ENTER",
            "ENTER",
            _TSTR("button_yes"),
            [library, playlistId, callback](const std::string& str) {
                auto query = std::make_shared<DeletePlaylistQuery>(library, playlistId);
                library->Enqueue(query, callback);
            });

    App::Overlays().Push(dialog);
}

static void showNoPlaylistsDialog() {
    auto dialog = std::make_shared<DialogOverlay>();

    (*dialog)
        .SetTitle(_TSTR("default_overlay_title"))
        .SetMessage(_TSTR("playqueue_overlay_load_playlists_none_message"))
        .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"));

    App::Overlays().Push(dialog);
}

static void handleAddCategorySelectionToPlayQueue(
    PlaybackService& playback,
    ILibraryPtr library,
    const std::string& fieldColumn,
    const std::string& fieldValue,
    int64_t fieldId,
    size_t type)
{
    const auto callback = [&playback, type](std::shared_ptr<TrackList> tracks) {
        if (tracks) {
            auto editor = playback.Edit();
            const size_t position = playback.GetIndex();

            if (type == 0) { /* start */
                for (size_t i = 0; i < tracks->Count(); i++) {
                    editor.Insert(tracks->GetId(i), i);
                }
            }
            else if (type == 1 || position == ListWindow::NO_SELECTION) { /* end */
                for (size_t i = 0; i < tracks->Count(); i++) {
                    editor.Add(tracks->GetId(i));
                }
            }
            else { /* after next */
                for (size_t i = 0; i < tracks->Count(); i++) {
                    editor.Insert(tracks->GetId(i), position + 1 + i);
                }
            }
        }
    };

    queryTracksByCategory(library, fieldColumn, fieldValue, fieldId, callback);
}

static void handleJumpTo(
    size_t index,
    IMessageQueue& messageQueue,
    TrackPtr track)
{
    int64_t type = -1LL;
    int64_t id = -1LL;

    if (index == 0) {
        type = cube::message::category::Album;
        id = track->GetInt64(library::constants::Track::ALBUM_ID);
    }
    else if (index == 1) {
        type = cube::message::category::Artist;
        id = track->GetInt64(library::constants::Track::ARTIST_ID);
    }
    else if (index == 2) {
        type = cube::message::category::Genre;
        id = track->GetInt64(library::constants::Track::GENRE_ID);
    }

    if (type != -1LL && id != -1LL) {
        messageQueue.Broadcast(runtime::Message::Create(
            nullptr, cube::message::JumpToCategory, type, id));
    }
}

static void showPlaylistPicker(const std::string& title, bool allowCreate,
    std::vector<api::PlaylistEntry> playlists, int64_t preferredId,
    PlayQueueOverlays::PlaylistPickedCallback picked)
{
    auto adapter = std::make_shared<Adapter>();
    adapter->SetSelectable(true);
    if (allowCreate) adapter->AddEntry(_TSTR("playqueue_overlay_new"));
    for (const auto& playlist : playlists) adapter->AddEntry(playlist.name);

    const auto preferred = std::find_if(playlists.begin(), playlists.end(),
        [preferredId](const auto& playlist) { return playlist.id == preferredId; });
    const size_t offset = allowCreate ? 1 : 0;
    const size_t selected = preferred == playlists.end() ? 0
        : static_cast<size_t>(std::distance(playlists.begin(), preferred)) + offset;

    showPlaylistListOverlay(title, adapter,
        [offset, picked, playlists = std::move(playlists)](ListOverlay*, IScrollAdapterPtr, size_t index) {
            if (index < offset) picked({0, {}});
            else if (index - offset < playlists.size()) picked(playlists[index - offset]);
        }, selected);
}

using LoadPlaylistTracks = std::function<void(std::function<void(std::shared_ptr<TrackList>)>)>;

static void showLegacyAddToPlaylistOverlay(
    IMessageQueue& queue, ILibraryPtr library, LoadPlaylistTracks load)
{
    queryPlaylists(library, [&queue, library, load](std::shared_ptr<CategoryListQuery> query) {
        if (!query) return;

        const auto result = query->GetResult();
        std::vector<api::PlaylistEntry> playlists;
        playlists.reserve(result->Count());
        for (size_t i = 0; i < result->Count(); ++i) {
            const auto entry = result->At(i);
            playlists.push_back({entry->GetId(), entry->ToString()});
        }

        showPlaylistPicker(_TSTR("playqueue_overlay_select_playlist_title"), true, std::move(playlists),
            lastOperationExpired() ? 0 : lastPlaylistId,
            [&queue, library, load](const api::PlaylistEntry& playlist) {
                load([&queue, library, id = playlist.id](std::shared_ptr<TrackList> tracks) {
                    if (!tracks) return;
                    if (id) {
                        setLastPlaylistId(id);
                        library->Enqueue(SavePlaylistQuery::Append(library, id, tracks));
                    }
                    else {
                        createNewPlaylist(queue, tracks, library);
                    }
                });
            });
    });
}

static void showAddCategorySelectionToPlaylistOverlay(
    IMessageQueue& queue, ILibraryPtr library, const std::string& field,
    const std::string& value, int64_t id)
{
    showLegacyAddToPlaylistOverlay(queue, library, [library, field, value, id](auto loaded) {
        queryTracksByCategory(library, field, value, id, loaded);
    });
}

PlayQueueOverlays::PlayQueueOverlays() noexcept {
}

void PlayQueueOverlays::ShowAddTrackOverlay(
    IMessageQueue& messageQueue,
    ILibraryPtr library,
    PlaybackService& playback,
    TrackPtr track,
    std::shared_ptr<const api::TrackRef> trackRef,
    AddTrackToPlaylistCallback addToPlaylist)
{
    enum TrackActionIndex : size_t {
        JumpToAlbum, JumpToArtist, JumpToGenre, AddToPlaylist, Prepend, Append, PlayNext, PlayNow
    };

    auto adapter = std::make_shared<Adapter>();
    adapter->AddEntry(_TSTR("playqueue_overlay_album_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_artist_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_genre_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_playlist"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_start_of_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_end_in_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_as_next_in_queue"));
    if (trackRef) adapter->AddEntry(_TSTR("playqueue_overlay_track_play_now"));
    adapter->SetSelectable(true);

    size_t selectedIndex = 0;
    if (!lastOperationExpired()) {
        selectedIndex = std::min(lastTrackOverlayIndex, size_t(trackRef ? PlayNow : PlayNext));
    }

    auto dialog = std::make_shared<ListOverlay>();

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playqueue_overlay_track_actions_title"))
        .SetWidth(_DIMEN("playqueue_playlist_add_to_queue_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [track, trackRef, library, addToPlaylist, &messageQueue, &playback]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (index > (trackRef ? PlayNow : PlayNext)) {
                    return;
                }

                setLastTrackOverlayIndex(index);

                if (index == AddToPlaylist && addToPlaylist) {
                    addToPlaylist(trackRef ? trackRef->Id() : track->GetId());
                    return;
                }

                if (trackRef && index >= Prepend) {
                    const mc_playback_track_action actions[] = {
                        MC_PLAYBACK_TRACK_PREPEND, MC_PLAYBACK_TRACK_APPEND,
                        MC_PLAYBACK_TRACK_PLAY_NEXT, MC_PLAYBACK_TRACK_PLAY_NOW
                    };
                    musik::core::audio::CreatePlaybackHandle(playback).UseTrack(*trackRef, actions[index - Prepend]);
                    return;
                }

                /* The remaining menu actions still consume legacy track metadata. */
                auto selected = track;
                if (trackRef) {
                    const auto id = trackRef->Id();
                    selected = std::make_shared<TrackList>(library, &id, 1)->Get(0);
                }
                if (!selected) return;

                if (index <= JumpToGenre) {
                    handleJumpTo(index, messageQueue, selected);
                    return;
                }
                if (index == AddToPlaylist) {
                    showLegacyAddToPlaylistOverlay(messageQueue, library, [library, selected](auto loaded) {
                        const auto id = selected->GetId();
                        loaded(std::make_shared<TrackList>(library, &id, 1));
                    });
                    return;
                }

                auto editor = playback.Edit();
                if (index == Prepend) {
                    editor.Insert(selected->GetId(), 0);
                }
                else if (index == Append) {
                    editor.Add(selected->GetId());
                }
                else { /* next */
                    const size_t position = playback.GetIndex();
                    if (position == ListWindow::NO_SELECTION) {
                        editor.Add(selected->GetId());
                    }
                    else {
                        editor.Insert(selected->GetId(), position + 1);
                    }
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowAddCategoryOverlay(
    IMessageQueue& messageQueue,
    PlaybackService& playback,
    ILibraryPtr library,
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
            [&playback, &messageQueue, library, fieldColumn, fieldValue, fieldId, addToPlaylist]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (index == ListWindow::NO_SELECTION) {
                    return;
                }

                setLastCategoryOverlayIndex(index);

                if (index == 0) {
                    if (addToPlaylist) addToPlaylist(fieldColumn, fieldValue, fieldId);
                    else showAddCategorySelectionToPlaylistOverlay(
                            messageQueue, library, fieldColumn, fieldValue, fieldId);
                }
                else if (index == adapter->GetEntryCount() - 1) {
                    auto const callback = [&playback](std::shared_ptr<TrackList> tracks) {
                        playback.HotSwap(*tracks);
                    };

                    queryTracksByCategory(library, fieldColumn, fieldValue, fieldId, callback);
                }
                else {
                    handleAddCategorySelectionToPlayQueue(
                        playback, library, fieldColumn, fieldValue, fieldId, index - 1);
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowAddDirectoryOverlay(
    musik::core::runtime::IMessageQueue& messageQueue,
    musik::core::audio::PlaybackService& playback,
    musik::core::ILibraryPtr library,
    const std::string& directory)
{
    ShowAddCategoryOverlay(
        messageQueue,
        playback,
        library,
        MagicConstants::DirectoryCategoryType,
        directory,
        -1LL);
}

void PlayQueueOverlays::ShowAlbumDividerOverlay(
    IMessageQueue& messageQueue, PlaybackService& playback, ILibraryPtr library, TrackPtr firstTrack,
    AddCategoryToPlaylistCallback addToPlaylist)
{
    if (!firstTrack) return;
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
            [&playback, library, &messageQueue, firstTrack, addToPlaylist]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
            if (index == ListWindow::NO_SELECTION) {
                return;
            }

            auto albumColumn = library::constants::Track::ALBUM;
            auto const albumId = firstTrack->GetInt64(library::constants::Track::ALBUM_ID);

            /* items 0, 1, and 2 jump to category */
            if (index <= 2) {
                handleJumpTo(index, messageQueue, firstTrack);
            }
            else if (index == 3) { /* replace play queue */
                auto query = std::make_shared<CategoryTrackListQuery>(library, albumColumn, albumId);
                library->Enqueue(query, [&playback, query](auto q) {
                    if (query->GetStatus() == IQuery::Finished) {
                        playback.Play(*query->GetResult().get(), 0);
                    }
                });
            }
            /* add to playlist */
            else if (index == 4) {
                if (addToPlaylist) addToPlaylist(albumColumn, "", albumId);
                else showAddCategorySelectionToPlaylistOverlay(
                        messageQueue, library, albumColumn, "", albumId);
            }
            /* the other are our standard play queue operations */
            else {
                handleAddCategorySelectionToPlayQueue(
                    playback, library, albumColumn, "", albumId, index - 5);
            }
    });

    cursespp::App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowPlaylistPicker(api::Context& context, const api::Library& library,
    const std::string& title, bool allowCreate, RequestGeneration::Ticket ticket, PlaylistPickedCallback picked,
    int64_t preferredId)
{
    api::ListPlaylists(context, library,
        [title, allowCreate, ticket, picked, preferredId](mc_result status, std::vector<api::PlaylistEntry> playlists) {
            if (!ticket.IsCurrent() || status != MC_OK) return;
            if (!allowCreate && playlists.empty()) {
                showNoPlaylistsDialog();
                return;
            }

            showPlaylistPicker(title, allowCreate, std::move(playlists), preferredId,
                [ticket, picked](const api::PlaylistEntry& playlist) {
                    if (ticket.IsCurrent()) picked(playlist);
                });
        });
}

void PlayQueueOverlays::ShowAddToPlaylistPicker(std::weak_ptr<api::Context> context,
    std::shared_ptr<const api::Library> library, RequestGeneration::Ticket ticket, std::vector<int64_t> ids)
{
    const auto dispatcher = context.lock();
    if (!dispatcher || !ticket.IsCurrent() || ids.empty()) return;

    const auto tracks = std::make_shared<const std::vector<int64_t>>(std::move(ids));
    const auto write = [context, library, ticket, tracks](int64_t id, const std::string& name) {
        const auto dispatcher = context.lock();
        if (!dispatcher || !ticket.IsCurrent()) return;
        const auto complete = [ticket](mc_result status, int64_t playlistId) {
            if (ticket.IsCurrent() && status == MC_OK && playlistId > 0) setLastPlaylistId(playlistId);
        };
        try {
            if (id) api::AppendPlaylist(*dispatcher, *library, id, *tracks, complete);
            else api::SavePlaylist(*dispatcher, *library, 0, name, *tracks, complete);
        }
        catch (const api::Error& error) {
            musik::debug::error("PlayQueueOverlays", error.what());
        }
    };

    try {
        ShowPlaylistPicker(*dispatcher, *library, _TSTR("playqueue_overlay_select_playlist_title"), true, ticket,
            [write](const api::PlaylistEntry& playlist) {
                if (playlist.id) write(playlist.id, {});
                else ShowPlaylistNameOverlay(_TSTR("playqueue_overlay_playlist_name_title"), {},
                    [write](const std::string& name) { write(0, name); });
            }, lastOperationExpired() ? 0 : lastPlaylistId);
    }
    catch (const api::Error& error) {
        musik::debug::error("PlayQueueOverlays", error.what());
    }
}

void PlayQueueOverlays::ShowAddCategoryToPlaylistPicker(std::weak_ptr<api::Context> context,
    std::shared_ptr<const api::Library> library, RequestGeneration::Ticket ticket,
    ILibraryPtr backend, const std::string& field, const std::string& value, int64_t id)
{
    if (!ticket.IsCurrent()) return;
    /* Resolve the ordered selection through the concrete backend that supplied the category. */
    queryTracksByCategory(backend, field, value, id,
        [context, library, ticket](std::shared_ptr<TrackList> tracks) {
            if (tracks && ticket.IsCurrent()) ShowAddToPlaylistPicker(context, library, ticket, tracks->GetIds());
        });
}

void PlayQueueOverlays::ShowPlaylistNameOverlay(const std::string& title, const std::string& name,
    std::function<void(const std::string&)> accepted)
{
    auto dialog = std::make_shared<InputOverlay>();
    dialog->SetTitle(title)
        .SetWidth(_DIMEN("playqueue_playlist_name_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetText(name)
        .SetInputAcceptedCallback([accepted](const std::string& value) {
            if (!value.empty()) accepted(value);
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

void PlayQueueOverlays::ShowCreatePlaylistOverlay(
    IMessageQueue& queue, ILibraryPtr library, QueryCallback callback)
{
    auto tracks = std::make_shared<TrackList>(library);
    createNewPlaylist(queue, tracks, library, callback);
}
