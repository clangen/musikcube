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

#include <musikcore/library/LibraryBackend.h>
#include <musikcore/plugin/library.h>
#include <musikcore/ffi/plugin_abi/PluginRegistry.h>
#include <mutex>

namespace musik { namespace core { namespace library {
    class Library;
} } }
namespace musik { namespace core { namespace plugin {

    /* implements the plugin library C service table. it guards plugin callback
    lifetimes and cancellation while using the selected native library instance.
    public application C++ wrappers live in library.hpp and own C handles only. */
    class PluginLibraryService {
    public:
        PluginLibraryService(mc_context& context, musik::core::LibraryBackend::Shared library,
            musik::core::ffi::PluginRegistry::Factory<mc_playback_remote_factory> factory);

        const mc_plugin_library_service& Api() const { return this->api; }

        /* wake remote reads before the owning plugin joins its workers. */
        void CancelReads() noexcept;

    private:
        static mc_result MC_CALL SearchTracks(
            void* context, const mc_track_search* search, int32_t limit, int32_t offset, mc_track_result** result);

        static mc_result MC_CALL TracksByExternalIds(
            void* context, const mc_string_view* ids, size_t count, mc_track_result** result);

        static mc_result MC_CALL TracksByIds(void* context, const int64_t* ids, size_t count, mc_track_result** result);

        static mc_result MC_CALL TracksByCategory(void* context, mc_string_view field, int64_t id,
            mc_string_view filter, int32_t limit, int32_t offset, mc_track_result** result);

        static mc_result MC_CALL TracksByCategories(void* context, const mc_library_category_selection* selections,
            size_t selectionCount, mc_string_view filter, mc_track_search_order order, int32_t limit, int32_t offset,
            mc_track_result** result);

        static mc_result MC_CALL ReadTracks(
            void* context, const mc_track_result* tracks, const mc_track_range* range, mc_track_page** page);

        static mc_result MC_CALL ReadTracksAsync(void* context, const mc_track_result* tracks,
            const mc_track_range* range, mc_track_query_completion_fn completion, void* userData);

        static mc_result MC_CALL RunQuery(
            void* context, mc_string_view queryJson, mc_library_text_fn completion, void* userData);

        static mc_result MC_CALL ListCategoryFields(
            void* context, mc_category_fields_completion_fn completion, void* userData);

        static mc_result MC_CALL ListCategoriesHost(void* context, mc_string_view field, mc_string_view filter,
            mc_track_search_mode mode, mc_category_list_completion_fn completion, void* userData);

        static mc_result MC_CALL ListFilteredCategories(void* context, mc_string_view field,
            const mc_library_category_selection* selections, size_t selectionCount, mc_string_view filter,
            mc_track_search_mode mode, bool facet, mc_string_view trackSearch,
            mc_category_list_completion_fn completion, void* userData);

        static mc_result MC_CALL ListAlbumsHost(void* context, mc_string_view filter,
            const mc_library_category_selection* selections, size_t selectionCount, mc_library_album_list_fn completion,
            void* userData);

        static mc_result MC_CALL ReadThumbnail(
            void* context, int64_t thumbnailId, mc_thumbnail_completion_fn completion, void* userData);

        static mc_result MC_CALL ReadLibraryThumbnail(void* context, const mc_library* library, int64_t thumbnailId,
            mc_thumbnail_completion_fn completion, void* userData);

        static mc_result MC_CALL SavePlaylistHost(void* context, int64_t playlistId, mc_string_view name,
            const int64_t* trackIds, size_t count, int64_t* savedId);

        static mc_result MC_CALL RenamePlaylistHost(void* context, int64_t playlistId, mc_string_view name);

        static mc_result MC_CALL AppendPlaylistHost(
            void* context, int64_t playlistId, const int64_t* trackIds, size_t count, int32_t offset);

        static mc_result MC_CALL RemovePlaylistEntries(void* context, int64_t playlistId,
            const mc_library_playlist_entry_key* entries, size_t count, size_t* removed);

        static mc_result MC_CALL DeletePlaylistHost(void* context, int64_t playlistId);

        mc_context& context;
        std::shared_ptr<const musik::core::library::Library> instance;
        musik::core::ffi::PluginRegistry::Factory<mc_playback_remote_factory> factory;
        std::mutex admissionMutex;
        bool stopping { false };
        std::atomic_bool cancelled { false };
        mc_plugin_library_service api;
    };

} } }