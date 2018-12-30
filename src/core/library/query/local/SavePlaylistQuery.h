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

#include <core/library/query/local/LocalQueryBase.h>
#include <core/library/track/TrackList.h>
#include <core/db/Connection.h>
#include <core/library/ILibrary.h>
#include <memory>

namespace musik { namespace core { namespace db { namespace local {

    class SavePlaylistQuery : public musik::core::db::LocalQueryBase {
        public:
            static std::shared_ptr<SavePlaylistQuery> Save(
                musik::core::ILibraryPtr library,
                const std::string& playlistName,
                std::shared_ptr<musik::core::TrackList> tracks);

            static std::shared_ptr<SavePlaylistQuery> Save(
                musik::core::ILibraryPtr library,
                const std::string& playlistName,
                musik::core::sdk::ITrackList* tracks);

            static std::shared_ptr<SavePlaylistQuery> Save(
                musik::core::ILibraryPtr library,
                const std::string& playlistName,
                const std::string& categoryType,
                int64_t categoryId);

            static std::shared_ptr<SavePlaylistQuery> Replace(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                std::shared_ptr<musik::core::TrackList> tracks);

            static std::shared_ptr<SavePlaylistQuery> Replace(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                musik::core::sdk::ITrackList* tracks);

            static std::shared_ptr<SavePlaylistQuery> Rename(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                const std::string& playlistName);

            static std::shared_ptr<SavePlaylistQuery> Append(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                std::shared_ptr<musik::core::TrackList> tracks);

            static std::shared_ptr<SavePlaylistQuery> Append(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                const std::string& categoryType,
                int64_t categoryId);

            virtual std::string Name() { return "SavePlaylistQuery"; }

            virtual ~SavePlaylistQuery();

            int64_t GetPlaylistId() const;

        protected:
            virtual bool OnRun(musik::core::db::Connection &db);

        private:
            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const std::string& playlistName,
                std::shared_ptr<musik::core::TrackList> tracks);

            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const std::string& playlistName,
                musik::core::sdk::ITrackList* tracks);

            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const std::string& playlistName,
                const std::string& categoryType,
                int64_t categoryId);

            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                std::shared_ptr<musik::core::TrackList> tracks);

            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                musik::core::sdk::ITrackList* tracks);

            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                const std::string& categoryType,
                int64_t categoryId);

            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                const std::string& newName);

            struct TrackListWrapper {
                TrackListWrapper();
                TrackListWrapper(std::shared_ptr<musik::core::TrackList> shared);

                bool Exists();
                size_t Count();
                TrackPtr Get(musik::core::ILibraryPtr library, size_t index);

                std::shared_ptr<musik::core::TrackList> sharedTracks;
                musik::core::sdk::ITrackList* rawTracks;
            };

            enum Operation { CreateOp, RenameOp, ReplaceOp, AppendOp };

            bool CreatePlaylist(musik::core::db::Connection &db);
            bool RenamePlaylist(musik::core::db::Connection &db);
            bool ReplacePlaylist(musik::core::db::Connection &db);
            bool AppendToPlaylist(musik::core::db::Connection &db);

            bool AddCategoryTracksToPlaylist(musik::core::db::Connection &db, int64_t playlistId);

            bool AddTracksToPlaylist(
                musik::core::db::Connection &db,
                int64_t playlistId,
                TrackListWrapper& tracks);

            Operation op;
            musik::core::ILibraryPtr library;
            std::string playlistName, categoryType;
            int64_t playlistId, categoryId;
            TrackListWrapper tracks;
    };

} } } }
