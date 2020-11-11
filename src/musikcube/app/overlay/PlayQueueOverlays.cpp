//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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
    auto dialog = std::make_shared<ListOverlay>();

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
    auto dialog = std::make_shared<DialogOverlay>();

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

static void showAddCategorySelectionToPlaylistOverlay(
    IMessageQueue& queue,
    ILibraryPtr library,
    const std::string& categoryType,
    const std::string& categoryValue,
    int64_t categoryId)
{
    auto playlistsCallback =
        [&queue, library, categoryType, categoryValue, categoryId]
        (std::shared_ptr<CategoryListQuery> playlistQuery)
    {
        auto result = playlistQuery->GetResult();

        auto adapter = std::make_shared<Adapter>();
        adapter->SetSelectable(true);
        adapter->AddEntry(_TSTR("playqueue_overlay_new"));
        addPlaylistsToAdapter(adapter, result);

        size_t selectedIndex = 0;
        if (!lastOperationExpired() && lastPlaylistId >= 0) {
            const int index = findPlaylistIndex(result, lastPlaylistId);
            if (index >= 0) {
                selectedIndex = (size_t) index + 1; /* +1 offsets "new..." */
            }
        }

        showPlaylistListOverlay(
            _TSTR("playqueue_overlay_select_playlist_title"),
            adapter,
            [&queue, library, categoryType, categoryValue, categoryId, result]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (index != ListWindow::NO_SELECTION) {
                    auto tracksCallback = [&queue, library, index, result](std::shared_ptr<TrackList> tracks) {
                        if (tracks) {
                            if (index == 0) { /* new playlist */
                                createNewPlaylist(queue, tracks, library);
                            }
                            else { /* add to existing */
                                int64_t playlistId = (*result)[index - 1]->GetId();
                                setLastPlaylistId(playlistId);
                                library->Enqueue(SavePlaylistQuery::Append(library, playlistId, tracks));
                            }
                        }
                    };

                    queryTracksByCategory(library, categoryType, categoryValue, categoryId, tracksCallback);
                }
            },
            selectedIndex);
    };

    queryPlaylists(library, playlistsCallback);
}

static void showAddTrackToPlaylistOverlay(
    IMessageQueue& queue, ILibraryPtr library, TrackPtr track)
{
    auto playlistsCallback = [&queue, library, track](std::shared_ptr<CategoryListQuery> playlistQuery) {
        auto result = playlistQuery->GetResult();

        auto adapter = std::make_shared<Adapter>();
        adapter->SetSelectable(true);
        adapter->AddEntry(_TSTR("playqueue_overlay_new"));
        addPlaylistsToAdapter(adapter, result);

        size_t selectedIndex = 0;
        if (!lastOperationExpired() && lastPlaylistId >= 0) {
            const int index = findPlaylistIndex(result, lastPlaylistId);
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
                    auto list = std::make_shared<TrackList>(library);
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
    };

    queryPlaylists(library, playlistsCallback);
}

PlayQueueOverlays::PlayQueueOverlays() noexcept {
}

void PlayQueueOverlays::ShowAddTrackOverlay(
    IMessageQueue& messageQueue,
    ILibraryPtr library,
    PlaybackService& playback,
    TrackPtr track)
{
    auto adapter = std::make_shared<Adapter>();
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

    auto dialog = std::make_shared<ListOverlay>();

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
                    const size_t position = playback.GetIndex();
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
            [&playback, &messageQueue, library, fieldColumn, fieldValue, fieldId]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (index == ListWindow::NO_SELECTION) {
                    return;
                }

                setLastCategoryOverlayIndex(index);

                if (index == 0) {
                    showAddCategorySelectionToPlaylistOverlay(
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
    IMessageQueue& messageQueue, PlaybackService& playback, ILibraryPtr library, TrackPtr firstTrack)
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
            [&playback, library, &messageQueue, firstTrack]
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
                showAddCategorySelectionToPlaylistOverlay(
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

void PlayQueueOverlays::ShowLoadPlaylistOverlay(
    PlaybackService& playback, ILibraryPtr library, PlaylistSelectedCallback selectedCallback)
{
    auto playlistsCallback =
        [&playback, library, selectedCallback]
        (std::shared_ptr<CategoryListQuery> playlistQuery)
    {
        auto result = playlistQuery->GetResult();

        if (!result->Count()) {
            showNoPlaylistsDialog();
            return;
        }

        auto adapter = std::make_shared<Adapter>();
        adapter->SetSelectable(true);
        addPlaylistsToAdapter(adapter, result);

        showPlaylistListOverlay(
            _TSTR("playqueue_overlay_load_playlist_title"),
            adapter,
            [library, result, selectedCallback]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (index != ListWindow::NO_SELECTION && selectedCallback) {
                    int64_t playlistId = (*result)[index]->GetId();
                    selectedCallback(playlistId);
                }
            });
    };

    queryPlaylists(library, playlistsCallback);
}

void PlayQueueOverlays::ShowSavePlaylistOverlay(
    IMessageQueue& queue,
    PlaybackService& playback,
    ILibraryPtr library,
    int64_t selectedPlaylistId)
{
    auto playlistsCallback =
        [&queue, &playback, library, selectedPlaylistId]
        (std::shared_ptr<CategoryListQuery> playlistQuery)
    {
        auto result = playlistQuery->GetResult();

        auto adapter = std::make_shared<Adapter>();
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
                auto tracks = std::make_shared<TrackList>(library);
                playback.CopyTo(*tracks);
                if (index == 0) { /* create new */
                    createNewPlaylist(queue, tracks, library);
                }
                else { /* replace existing */
                    --index;
                    int64_t playlistId = (*result)[index]->GetId();
                    const std::string playlistName = (*result)[index]->ToString();
                    confirmOverwritePlaylist(library, playlistName, playlistId, tracks);
                }
            },
            selectedIndex);
    };

    queryPlaylists(library, playlistsCallback);
}

void PlayQueueOverlays::ShowRenamePlaylistOverlay(ILibraryPtr library) {
    auto playlistsCallback = [library](std::shared_ptr<CategoryListQuery> playlistQuery) {
        auto result = playlistQuery->GetResult();

        if (!result->Count()) {
            showNoPlaylistsDialog();
            return;
        }

        auto adapter = std::make_shared<Adapter>();
        adapter->SetSelectable(true);
        addPlaylistsToAdapter(adapter, result);

        showPlaylistListOverlay(
            _TSTR("playqueue_overlay_rename_playlist_title"),
            adapter,
            [library, result](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (index != ListWindow::NO_SELECTION) {
                    int64_t playlistId = (*result)[index]->GetId();
                    const std::string playlistName = (*result)[index]->ToString();
                    ShowRenamePlaylistOverlay(library, playlistId, playlistName);
                }
            });
    };

    queryPlaylists(library, playlistsCallback);
}

void PlayQueueOverlays::ShowDeletePlaylistOverlay(ILibraryPtr library) {
    auto playlistsCallback = [library](std::shared_ptr<CategoryListQuery> playlistQuery) {
        auto result = playlistQuery->GetResult();

        if (!result->Count()) {
            showNoPlaylistsDialog();
            return;
        }

        auto adapter = std::make_shared<Adapter>();
        adapter->SetSelectable(true);
        addPlaylistsToAdapter(adapter, result);

        showPlaylistListOverlay(
            _TSTR("playqueue_overlay_delete_playlist_title"),
            adapter,
            [library, result]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (index != ListWindow::NO_SELECTION) {
                    int64_t playlistId = (*result)[index]->GetId();
                    const std::string playlistName = (*result)[index]->ToString();
                    ShowConfirmDeletePlaylistOverlay(library, playlistName, playlistId);
                }
            });
    };

    queryPlaylists(library, playlistsCallback);
}

void PlayQueueOverlays::ShowCreatePlaylistOverlay(
    IMessageQueue& queue, ILibraryPtr library, QueryCallback callback)
{
    auto tracks = std::make_shared<TrackList>(library);
    createNewPlaylist(queue, tracks, library, callback);
}