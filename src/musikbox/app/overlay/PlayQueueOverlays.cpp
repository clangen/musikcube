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

#include "stdafx.h"

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

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::library::constants;
using namespace musik::box;
using namespace cursespp;

using Adapter = cursespp::SimpleScrollAdapter;

static std::string stringWithFormat(const std::string& key, const std::string& value) {
    std::string message = _TSTR(key);
    try {
        message = boost::str(boost::format(message) % value);
    }
    catch (...) {
    }
    return message;
}

static std::shared_ptr<CategoryListQuery> queryPlaylists(musik::core::ILibraryPtr library) {
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
    std::shared_ptr<Adapter> adapter, CategoryListQuery::ResultList result)
{
    auto it = result->begin();
    while (it != result->end()) {
        adapter->AddEntry((*it)->displayValue);
        ++it;
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
        .SetWidth(_DIMEN("playqueue_playlist_list_overlay", DEFAULT_OVERLAY_WIDTH))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(callback);

    cursespp::App::Overlays().Push(dialog);
}

static void confirmOverwritePlaylist(
    musik::core::ILibraryPtr library,
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
                library->Enqueue(SavePlaylistQuery::Replace(playlistId, tracks));
            });

    App::Overlays().Push(dialog);
}

static void createNewPlaylist(
    std::shared_ptr<TrackList> tracks,
    musik::core::ILibraryPtr library,
    musik::core::ILibrary::Callback callback = musik::core::ILibrary::Callback())
{
    std::shared_ptr<InputOverlay> dialog(new InputOverlay());

    dialog->SetTitle(_TSTR("playqueue_overlay_playlist_name_title"))
        .SetWidth(_DIMEN("playqueue_playlist_name_overlay", DEFAULT_OVERLAY_WIDTH))
        .SetText("")
        .SetInputAcceptedCallback(
            [tracks, library, callback](const std::string& name) {
                if (name.size()) {
                    library->Enqueue(SavePlaylistQuery::Save(name, tracks), 0, callback);
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

static void createNewPlaylist(
    musik::core::ILibraryPtr library,
    const std::string& categoryType,
    int64_t categoryId)
{
    std::shared_ptr<InputOverlay> dialog(new InputOverlay());

    dialog->SetTitle(_TSTR("playqueue_overlay_playlist_name_title"))
        .SetWidth(_DIMEN("playqueue_playlist_name_overlay", DEFAULT_OVERLAY_WIDTH))
        .SetText("")
        .SetInputAcceptedCallback(
            [library, categoryType, categoryId](const std::string& name) {
        if (name.size()) {
            library->Enqueue(SavePlaylistQuery::Save(
                library, name, categoryType, categoryId));
        }
    });

    cursespp::App::Overlays().Push(dialog);
}

static void renamePlaylist(
    musik::core::ILibraryPtr library,
    const int64_t playlistId,
    const std::string& oldName)
{
    std::shared_ptr<InputOverlay> dialog(new InputOverlay());

    dialog->SetTitle(_TSTR("playqueue_overlay_new_playlist_name_title"))
        .SetWidth(_DIMEN("playqueue_playlist_name_overlay", DEFAULT_OVERLAY_WIDTH))
        .SetText(oldName)
        .SetInputAcceptedCallback(
            [library, playlistId](const std::string& name) {
                if (name.size()) {
                    library->Enqueue(SavePlaylistQuery::Rename(playlistId, name));
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

static void confirmDeletePlaylist(
    musik::core::ILibraryPtr library,
    const std::string& playlistName,
    const int64_t playlistId)
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
            [library, playlistId](const std::string& str) {
                library->Enqueue(std::shared_ptr<DeletePlaylistQuery>(
                    new DeletePlaylistQuery(playlistId)));
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
    musik::core::audio::PlaybackService& playback,
    musik::core::ILibraryPtr library,
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
    musik::core::runtime::IMessageQueue& messageQueue,
    musik::core::TrackPtr track)
{
    int64_t type;
    int64_t id;

    if (index == 0) {
        type = message::category::Album;
        id = track->GetInt64(library::constants::Track::ALBUM_ID);
    }
    else if (index == 1) {
        type = message::category::Artist;
        id = track->GetInt64(library::constants::Track::ARTIST_ID);
    }
    else if (index == 2) {
        type = message::category::Genre;
        id = track->GetInt64(library::constants::Track::GENRE_ID);
    }

    messageQueue.Broadcast(runtime::Message::Create(
        nullptr, message::JumpToCategory, type, id));
}

static void showAddCategorySelectionToPlaylistOverlay(
    musik::core::ILibraryPtr library, const std::string& categoryType, int64_t categoryId)
{
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->SetSelectable(true);
    adapter->AddEntry(_TSTR("playqueue_overlay_new"));
    addPlaylistsToAdapter(adapter, result);

    showPlaylistListOverlay(
        _TSTR("playqueue_overlay_select_playlist_title"),
        adapter,
        [library, categoryType, categoryId, result]
        (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
            if (index != ListWindow::NO_SELECTION) {
                if (index == 0) { /* new playlist */
                    createNewPlaylist(library, categoryType, categoryId);
                }
                else { /* add to existing */
                    int64_t playlistId = (*result)[index - 1]->id;

                    library->Enqueue(SavePlaylistQuery::Append(
                        library, playlistId, categoryType, categoryId));
                }
            }
    });
}

static void showAddTrackToPlaylistOverlay(
    musik::core::ILibraryPtr library, musik::core::TrackPtr track)
{
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->SetSelectable(true);
    adapter->AddEntry(_TSTR("playqueue_overlay_new"));
    addPlaylistsToAdapter(adapter, result);

    showPlaylistListOverlay(
        _TSTR("playqueue_overlay_select_playlist_title"),
        adapter,
        [library, track, result]
        (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
            if (index != ListWindow::NO_SELECTION) {
                std::shared_ptr<TrackList> list(new TrackList(library));
                list->Add(track->GetId());

                if (index == 0) { /* new playlist */
                    createNewPlaylist(list, library);
                }
                else { /* add to existing */
                    int64_t playlistId = (*result)[index - 1]->id;
                    library->Enqueue(SavePlaylistQuery::Append(playlistId, list));
                }
            }
    });
}

PlayQueueOverlays::PlayQueueOverlays() {
}

void PlayQueueOverlays::ShowAddTrackOverlay(
    runtime::IMessageQueue& messageQueue,
    musik::core::ILibraryPtr library,
    PlaybackService& playback,
    musik::core::TrackPtr track)
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

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playqueue_overlay_track_actions_title"))
        .SetWidth(_DIMEN("playqueue_playlist_add_to_queue_overlay", DEFAULT_OVERLAY_WIDTH))
        .SetSelectedIndex(0)
        .SetItemSelectedCallback(
            [track, library, &messageQueue, &playback]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                auto editor = playback.Edit();
                if (index <= 2) {
                    handleJumpTo(index, messageQueue, track);
                }
                else if (index == 3) {
                    showAddTrackToPlaylistOverlay(library, track);
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
    musik::core::audio::PlaybackService& playback,
    musik::core::ILibraryPtr library,
    const std::string& fieldColumn,
    int64_t fieldId)
{
    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_playlist"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_start"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_to_end"));
    adapter->AddEntry(_TSTR("playqueue_overlay_add_as_next"));
    adapter->SetSelectable(true);

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playqueue_overlay_add_to_queue_title"))
        .SetWidth(_DIMEN("playqueue_playlist_add_to_queue_overlay", DEFAULT_OVERLAY_WIDTH))
        .SetSelectedIndex(0)
        .SetItemSelectedCallback(
            [&playback, library, fieldColumn, fieldId]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (index == ListWindow::NO_SELECTION) {
                    return;
                }
                else if (index == 0) {
                    showAddCategorySelectionToPlaylistOverlay(library, fieldColumn, fieldId);
                }
                else {
                    handleAddCategorySelectionToPlayQueue(playback, library, fieldColumn, fieldId, index - 1);
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowAlbumDividerOverlay(
    musik::core::runtime::IMessageQueue& messageQueue,
    musik::core::audio::PlaybackService& playback,
    musik::core::ILibraryPtr library,
    musik::core::TrackPtr firstTrack)
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
        .SetWidth(_DIMEN("playqueue_album_header_overlay", DEFAULT_OVERLAY_WIDTH))
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
                showAddCategorySelectionToPlaylistOverlay(library, albumColumn, albumId);
            }
            /* the other are our standard play queue operations */
            else {
                handleAddCategorySelectionToPlayQueue(playback, library, albumColumn, albumId, index - 5);
            }
    });

    cursespp::App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowLoadPlaylistOverlay(
    musik::core::audio::PlaybackService& playback,
    musik::core::ILibraryPtr library,
    PlaylistSelectedCallback callback)
{
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    if (!result->size()) {
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
                int64_t playlistId = (*result)[index]->id;
                callback(playlistId);
            }
        });
}

void PlayQueueOverlays::ShowSavePlaylistOverlay(
    musik::core::audio::PlaybackService& playback,
    musik::core::ILibraryPtr library,
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
        for (size_t i = 0; i < result->size(); i++) {
            if (result->at(i)->id == selectedPlaylistId) {
                selectedIndex = i + 1; /* offset "new..." */
                break;
            }
        }
    }

    showPlaylistListOverlay(
        _TSTR("playqueue_overlay_save_playlist_title"),
        adapter,
        [&playback, library, result]
        (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
            std::shared_ptr<TrackList> tracks(new TrackList(library));
            playback.CopyTo(*tracks);

            if (index == 0) { /* create new */
                createNewPlaylist(tracks, library);
            }
            else { /* replace existing */
                --index;
                int64_t playlistId = (*result)[index]->id;
                std::string playlistName = (*result)[index]->displayValue;
                confirmOverwritePlaylist(library, playlistName, playlistId, tracks);
            }
        },
        selectedIndex);
}

void PlayQueueOverlays::ShowRenamePlaylistOverlay(musik::core::ILibraryPtr library) {
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    if (!result->size()) {
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
                int64_t playlistId = (*result)[index]->id;
                std::string playlistName = (*result)[index]->displayValue;
                renamePlaylist(library, playlistId, playlistName);
            }
        });
}

void PlayQueueOverlays::ShowDeletePlaylistOverlay(musik::core::ILibraryPtr library) {
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    if (!result->size()) {
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
                int64_t playlistId = (*result)[index]->id;
                std::string playlistName = (*result)[index]->displayValue;
                confirmDeletePlaylist(library, playlistName, playlistId);
            }
        });
}

void PlayQueueOverlays::ShowCreatePlaylistOverlay(
    musik::core::ILibraryPtr library, QueryCallback callback)
{
    createNewPlaylist(std::make_shared<TrackList>(library), library, callback);
}