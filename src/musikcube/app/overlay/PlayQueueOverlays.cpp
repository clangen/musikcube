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

#include <stdafx.h>

#include <chrono>

#include "PlayQueueOverlays.h"

#include <core/audio/Visualizer.h>
#include <core/library/LocalLibraryConstants.h>

#include <core/library/query/local/CategoryTrackListQuery.h>
#include <core/library/query/local/CategoryListQuery.h>
#include <core/library/query/local/GetPlaylistQuery.h>
#include <core/library/query/local/SavePlaylistQuery.h>
#include <core/library/query/local/DeletePlaylistQuery.h>
#include <core/runtime/Message.h>

#include <app/util/Messages.h>

#include <cursespp/App.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/InputOverlay.h>

#define DEFAULT_OVERLAY_WIDTH 30

using namespace musik;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::db;
using namespace musik::core::db::local;
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

static inline milliseconds now() {
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch());
}

static inline bool lastOperationExpired() {
    return now() > lastOperationExpiry;
}

static inline void touchOperationExpiry() {
    lastOperationExpiry = now() + duration_cast<milliseconds>(Seconds(60));
}

static inline int findPlaylistIndex(CategoryListQuery::Result result, int64_t playlistId) {
    for (size_t i = 0; i < result->Count(); i++) {
        if (result->At(i)->GetId() == playlistId) {
            return (int)i;
        }
    }
    return -1;
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

static std::shared_ptr<CategoryListQuery> queryPlaylists(ILibraryPtr library) {
    std::shared_ptr<CategoryListQuery> query(
        new CategoryListQuery(Playlists::TABLE_NAME, ""));

    library->Enqueue(query, ILibrary::QuerySynchronous);
    if (query->GetStatus() != IQuery::Finished) {
        query.reset();
        return query;
    }

    return query;
}

static void addPlaylistsToAdapter(
    std::shared_ptr<Adapter> adapter, CategoryListQuery::Result result)
{
    for (size_t i = 0; i < result->Count(); i++) {
        adapter->AddEntry(result->At(i)->ToString());
    }
}

static void showPlaylistListOverlay(
    const std::string& title,
    std::shared_ptr<Adapter> adapter,
    ListOverlay::ItemSelectedCallback callback,
    size_t selectedIndex = 0)
{
    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(title)
        .SetWidth(_DIMEN("playqueue_playlist_list_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(callback);

    cursespp::App::Overlays().Push(dialog);
}

static void confirmOverwritePlaylist(
    ILibraryPtr library,
    const std::string& playlistName,
    const int64_t playlistId,
    std::shared_ptr<TrackList> tracks)
{
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    (*dialog)
        .SetTitle(_TSTR("default_overlay_title"))
        .SetMessage(stringWithFormat(
            "playqueue_overlay_confirm_overwrite_message",
            playlistName))
        .AddButton("^[", "ESC", _TSTR("button_no"))
        .AddButton(
            "KEY_ENTER",
            "ENTER",
            _TSTR("button_yes"),
            [library, playlistId, tracks](const std::string& str) {
                library->Enqueue(SavePlaylistQuery::Replace(library, playlistId, tracks));
            });

    App::Overlays().Push(dialog);
}

static void createNewPlaylist(
    IMessageQueue& queue,
    std::shared_ptr<TrackList> tracks,
    ILibraryPtr library,
    ILibrary::Callback callback = ILibrary::Callback())
{
    std::shared_ptr<InputOverlay> dialog(new InputOverlay());

    dialog->SetTitle(_TSTR("playqueue_overlay_playlist_name_title"))
        .SetWidth(_DIMEN("playqueue_playlist_name_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetText("")
        .SetInputAcceptedCallback(
            [&queue, tracks, library, callback](const std::string& name) {
                if (name.size()) {
                    auto query = SavePlaylistQuery::Save(library, name, tracks);
                    library->Enqueue(query, ILibrary::QuerySynchronous, [&queue, callback](auto query) {
                        setLastPlaylistId(std::static_pointer_cast<SavePlaylistQuery>(query)->GetPlaylistId());
                        if (callback) {
                            callback(query);
                        }
                    });
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

static void createNewPlaylist(
    IMessageQueue& queue,
    ILibraryPtr library,
    const std::string& categoryType,
    int64_t categoryId)
{
    std::shared_ptr<InputOverlay> dialog(new InputOverlay());

    dialog->SetTitle(_TSTR("playqueue_overlay_playlist_name_title"))
        .SetWidth(_DIMEN("playqueue_playlist_name_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetText("")
        .SetInputAcceptedCallback(
            [&queue, library, categoryType, categoryId](const std::string& name) {
            if (name.size()) {
                auto query = SavePlaylistQuery::Save(library, name, categoryType, categoryId);
                library->Enqueue(query, ILibrary::QuerySynchronous, [](auto query) {
                    setLastPlaylistId(std::static_pointer_cast<SavePlaylistQuery>(query)->GetPlaylistId());
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
    std::shared_ptr<InputOverlay> dialog(new InputOverlay());

    dialog->SetTitle(_TSTR("playqueue_overlay_new_playlist_name_title"))
        .SetWidth(_DIMEN("playqueue_playlist_name_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetText(oldName)
        .SetInputAcceptedCallback(
            [library, playlistId, callback](const std::string& name) {
                if (name.size()) {
                    library->Enqueue(SavePlaylistQuery::Rename(library, playlistId, name), 0, callback);
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
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

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
                library->Enqueue(std::shared_ptr<DeletePlaylistQuery>(
                    new DeletePlaylistQuery(library, playlistId)), 0, callback);
            });

    App::Overlays().Push(dialog);
}

static void showNoPlaylistsDialog() {
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

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
    int64_t fieldId,
    size_t type)
{
    std::shared_ptr<CategoryTrackListQuery> query(
        new CategoryTrackListQuery(library, fieldColumn, fieldId));

    library->Enqueue(query, ILibrary::QuerySynchronous);

    if (query->GetStatus() == IQuery::Finished) {
        auto editor = playback.Edit();
        auto tracks = query->GetResult();
        size_t position = playback.GetIndex();

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
}

static void handleJumpTo(
    size_t index,
    IMessageQueue& messageQueue,
    TrackPtr track)
{
    int64_t type;
    int64_t id;

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

    messageQueue.Broadcast(runtime::Message::Create(
        nullptr, cube::message::JumpToCategory, type, id));
}

static void showAddCategorySelectionToPlaylistOverlay(
    IMessageQueue& queue,
    ILibraryPtr library,
    const std::string& categoryType,
    int64_t categoryId)
{
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->SetSelectable(true);
    adapter->AddEntry(_TSTR("playqueue_overlay_new"));
    addPlaylistsToAdapter(adapter, result);

    size_t selectedIndex = 0;
    if (!lastOperationExpired() && lastPlaylistId >= 0) {
        int index = findPlaylistIndex(result, lastPlaylistId);
        if (index >= 0) {
            selectedIndex = (size_t)index + 1; /* +1 offsets "new..." */
        }
    }

    showPlaylistListOverlay(
        _TSTR("playqueue_overlay_select_playlist_title"),
        adapter,
        [&queue, library, categoryType, categoryId, result]
        (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
            if (index != ListWindow::NO_SELECTION) {
                if (index == 0) { /* new playlist */
                    createNewPlaylist(queue, library, categoryType, categoryId);
                }
                else { /* add to existing */
                    int64_t playlistId = (*result)[index - 1]->GetId();
                    setLastPlaylistId(playlistId);

                    auto query = SavePlaylistQuery::Append(
                        library, playlistId, categoryType, categoryId);

                    library->Enqueue(query, 0);
                }
            }
        },
        selectedIndex);
}

static void showAddTrackToPlaylistOverlay(
    IMessageQueue& queue, ILibraryPtr library, TrackPtr track)
{
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->SetSelectable(true);
    adapter->AddEntry(_TSTR("playqueue_overlay_new"));
    addPlaylistsToAdapter(adapter, result);

    size_t selectedIndex = 0;
    if (!lastOperationExpired() && lastPlaylistId >= 0) {
        int index = findPlaylistIndex(result, lastPlaylistId);
        if (index >= 0) {
            selectedIndex = (size_t) index + 1; /* +1 offsets "new..." */
        }
    }

    showPlaylistListOverlay(
        _TSTR("playqueue_overlay_select_playlist_title"),
        adapter,
        [library, track, &queue, result]
        (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
            if (index != ListWindow::NO_SELECTION) {
                std::shared_ptr<TrackList> list(new TrackList(library));
                list->Add(track->GetId());

                if (index == 0) { /* new playlist */
                    createNewPlaylist(queue, list, library);
                }
                else { /* add to existing */
                    int64_t playlistId = (*result)[index - 1]->GetId();
                    setLastPlaylistId(playlistId);
                    library->Enqueue(SavePlaylistQuery::Append(library, playlistId, list), 0);
                }
            }
        },
        selectedIndex);
}

PlayQueueOverlays::PlayQueueOverlays() {
}

void PlayQueueOverlays::ShowAddTrackOverlay(
    IMessageQueue& messageQueue,
    ILibraryPtr library,
    PlaybackService& playback,
    TrackPtr track)
{
    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry(_TSTR("playqueue_overlay_album_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_artist_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_genre_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_playlist"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_start_of_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_end_in_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_as_next_in_queue"));
    adapter->SetSelectable(true);

    size_t selectedIndex = 0;
    if (!lastOperationExpired()) {
        selectedIndex = lastTrackOverlayIndex;
    }

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playqueue_overlay_track_actions_title"))
        .SetWidth(_DIMEN("playqueue_playlist_add_to_queue_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [track, library, &messageQueue, &playback]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (index == ListWindow::NO_SELECTION) {
                    return;
                }

                setLastTrackOverlayIndex(index);

                auto editor = playback.Edit();
                if (index <= 2) {
                    handleJumpTo(index, messageQueue, track);
                }
                else if (index == 3) {
                    showAddTrackToPlaylistOverlay(messageQueue, library, track);
                }
                else if (index == 4) { /* start */
                    editor.Insert(track->GetId(), 0);
                }
                else if (index == 5) { /* end */
                    editor.Add(track->GetId());
                }
                else { /* next */
                    size_t position = playback.GetIndex();
                    if (position == ListWindow::NO_SELECTION) {
                        editor.Add(track->GetId());
                    }
                    else {
                        editor.Insert(track->GetId(), position + 1);
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
    int64_t fieldId)
{
    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_playlist"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_start_of_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_end_in_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_as_next_in_queue"));
    adapter->SetSelectable(true);

    size_t selectedIndex = 0;
    if (!lastOperationExpired()) {
        selectedIndex = lastCategoryOverlayIndex;
    }

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playqueue_overlay_add_category_title"))
        .SetWidth(_DIMEN("playqueue_playlist_add_to_queue_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [&playback, &messageQueue, library, fieldColumn, fieldId]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (index == ListWindow::NO_SELECTION) {
                    return;
                }

                setLastCategoryOverlayIndex(index);

                if (index == 0) {
                    showAddCategorySelectionToPlaylistOverlay(
                        messageQueue, library, fieldColumn, fieldId);
                }
                else {
                    handleAddCategorySelectionToPlayQueue(
                        playback, library, fieldColumn, fieldId, index - 1);
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowAlbumDividerOverlay(
    IMessageQueue& messageQueue, PlaybackService& playback, ILibraryPtr library, TrackPtr firstTrack)
{
    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry(_TSTR("playqueue_overlay_album_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_artist_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_genre_jump_to"));
    adapter->AddEntry(_TSTR("playqueue_overlay_album_play"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_playlist"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_start_of_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_end_in_queue"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_as_next_in_queue"));
    adapter->SetSelectable(true);

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playqueue_overlay_album_header_actions_title"))
        .SetWidth(_DIMEN("playqueue_album_header_overlay_width", DEFAULT_OVERLAY_WIDTH))
        .SetSelectedIndex(0)
        .SetItemSelectedCallback(
            [&playback, library, &messageQueue, firstTrack]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
            if (index == ListWindow::NO_SELECTION) {
                return;
            }

            auto albumColumn = library::constants::Track::ALBUM;
            auto albumId = firstTrack->GetInt64(library::constants::Track::ALBUM_ID);

            /* items 0, 1, and 2 jump to category */
            if (index <= 2) {
                handleJumpTo(index, messageQueue, firstTrack);
            }
            else if (index == 3) { /* replace play queue */
                std::shared_ptr<CategoryTrackListQuery> query(
                    new CategoryTrackListQuery(library, albumColumn, albumId));

                library->Enqueue(query, ILibrary::QuerySynchronous);

                if (query->GetStatus() == IQuery::Finished) {
                    playback.Play(*query->GetResult().get(), 0);
                }
            }
            /* add to playlist */
            else if (index == 4) {
                showAddCategorySelectionToPlaylistOverlay(
                    messageQueue, library, albumColumn, albumId);
            }
            /* the other are our standard play queue operations */
            else {
                handleAddCategorySelectionToPlayQueue(
                    playback, library, albumColumn, albumId, index - 5);
            }
    });

    cursespp::App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowLoadPlaylistOverlay(
    PlaybackService& playback, ILibraryPtr library, PlaylistSelectedCallback callback)
{
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    if (!result->Count()) {
        showNoPlaylistsDialog();
        return;
    }

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->SetSelectable(true);
    addPlaylistsToAdapter(adapter, result);

    showPlaylistListOverlay(
        _TSTR("playqueue_overlay_load_playlist_title"),
        adapter,
        [library, result, callback]
        (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
            if (index != ListWindow::NO_SELECTION && callback) {
                int64_t playlistId = (*result)[index]->GetId();
                callback(playlistId);
            }
        });
}

void PlayQueueOverlays::ShowSavePlaylistOverlay(
    IMessageQueue& queue,
    PlaybackService& playback,
    ILibraryPtr library,
    int64_t selectedPlaylistId)
{
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->SetSelectable(true);
    adapter->AddEntry(_TSTR("playqueue_overlay_new"));
    addPlaylistsToAdapter(adapter, result);

    /* the caller can specify a playlistId that we should try to
    select by default. if they did, try to find it */
    size_t selectedIndex = 0;
    if (selectedPlaylistId != -1) {
        for (size_t i = 0; i < result->Count(); i++) {
            if (result->At(i)->GetId() == selectedPlaylistId) {
                selectedIndex = i + 1; /* offset "new..." */
                break;
            }
        }
    }

    showPlaylistListOverlay(
        _TSTR("playqueue_overlay_save_playlist_title"),
        adapter,
        [&queue, &playback, library, result]
        (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
            std::shared_ptr<TrackList> tracks(new TrackList(library));
            playback.CopyTo(*tracks);

            if (index == 0) { /* create new */
                createNewPlaylist(queue, tracks, library);
            }
            else { /* replace existing */
                --index;
                int64_t playlistId = (*result)[index]->GetId();
                std::string playlistName = (*result)[index]->ToString();
                confirmOverwritePlaylist(library, playlistName, playlistId, tracks);
            }
        },
        selectedIndex);
}

void PlayQueueOverlays::ShowRenamePlaylistOverlay(ILibraryPtr library) {
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    if (!result->Count()) {
        showNoPlaylistsDialog();
        return;
    }

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->SetSelectable(true);
    addPlaylistsToAdapter(adapter, result);

    showPlaylistListOverlay(
        _TSTR("playqueue_overlay_rename_playlist_title"),
        adapter,
        [library, result](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
            if (index != ListWindow::NO_SELECTION) {
                int64_t playlistId = (*result)[index]->GetId();
                std::string playlistName = (*result)[index]->ToString();
                ShowRenamePlaylistOverlay(library, playlistId, playlistName);
            }
        });
}

void PlayQueueOverlays::ShowDeletePlaylistOverlay(ILibraryPtr library) {
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    if (!result->Count()) {
        showNoPlaylistsDialog();
        return;
    }

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->SetSelectable(true);
    addPlaylistsToAdapter(adapter, result);

    showPlaylistListOverlay(
        _TSTR("playqueue_overlay_delete_playlist_title"),
        adapter,
        [library, result]
        (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
            if (index != ListWindow::NO_SELECTION) {
                int64_t playlistId = (*result)[index]->GetId();
                std::string playlistName = (*result)[index]->ToString();
                ShowConfirmDeletePlaylistOverlay(library, playlistName, playlistId);
            }
        });
}

void PlayQueueOverlays::ShowCreatePlaylistOverlay(
    IMessageQueue& queue, ILibraryPtr library, QueryCallback callback)
{
    auto tracks = std::make_shared<TrackList>(library);
    createNewPlaylist(queue, tracks, library, callback);
}