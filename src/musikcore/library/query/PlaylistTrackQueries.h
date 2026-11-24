#pragma once

#include <musikcore/library/query/TrackListQueryBase.h>
#include <musikcore/library/QueryBase.h>
#include <musikcore/db/ScopedTransaction.h>
#include <musikcore/db/Statement.h>
#include <common/runtime/Message.h>
#include <musikcore/support/Messages.h>
#include <map>
#include <string>
#include <vector>

namespace musik { namespace core { namespace library { namespace query {

/* resolves external track identifiers in their original order. */
class ExternalIdListToTrackListQuery : public TrackListQueryBase {
    public:
        ExternalIdListToTrackListQuery(
            ILibraryPtr library,
            const char** externalIds,
            size_t externalIdCount)
        {
            this->library = library;
            for (size_t i = 0; i < externalIdCount; ++i) {
                this->externalIds.emplace_back(externalIds[i]);
            }
        }

        std::shared_ptr<TrackList> GetResult() noexcept override {
            return this->result;
        }

        Headers GetHeaders() noexcept override {
            return Headers();
        }

        Durations GetDurations() noexcept override {
            return Durations();
        }

        size_t GetQueryHash() noexcept override {
            return 0;
        }

    protected:
        bool OnRun(musik::core::db::Connection& db) override {
            std::string sql = "SELECT id, external_id FROM tracks WHERE external_id IN(";
            for (size_t i = 0; i < this->externalIds.size(); i++) {
                sql += (i == 0) ? "?" : ",?";
            }
            sql += ");";

            db::Statement query(sql.c_str(), db);

            for (size_t i = 0; i < this->externalIds.size(); i++) {
                query.BindText((int) i, externalIds[i].c_str());
            }

            /* SQL does not preserve the input order, so restore it after lookup. */
            std::map<std::string, int64_t> records;

            while (query.Step() == db::Row) {
                records[query.ColumnText(1)] = query.ColumnInt64(0);
            }

            this->result = std::make_shared<TrackList>(this->library);
            auto end = records.end();
            for (size_t i = 0; i < this->externalIds.size(); i++) {
                auto r = records.find(externalIds[i]);
                if (r != end) {
                    this->result->Add(r->second);
                }
            }

            return true;
        }

        std::string Name() override {
            return "ExternalIdListToTrackListQuery";
        }

    private:
        ILibraryPtr library;
        std::vector<std::string> externalIds;
        std::shared_ptr<TrackList> result;
};

/* removes playlist entries and closes the gaps in their ordering. */
class RemoveFromPlaylistQuery : public QueryBase {
    public:
        RemoveFromPlaylistQuery(
            ILibraryPtr library,
            int64_t playlistId,
            const char** externalIds,
            const int* sortOrders,
            size_t count)
        {
            this->library = library;
            this->playlistId = playlistId;
            for (size_t i = 0; i < count; ++i) {
                this->externalIds.emplace_back(externalIds[i]);
                this->sortOrders.push_back(sortOrders[i]);
            }
            this->updated = 0;
        }

        size_t GetResult() noexcept {
            return this->updated;
        }

    protected:
        bool OnRun(musik::core::db::Connection& db) override {
            this->updated = 0;

            db::ScopedTransaction transaction(db);

            {
                db::Statement deleteStmt(
                    "DELETE FROM playlist_tracks "
                    "WHERE playlist_id=? AND track_external_id=? AND sort_order=?",
                    db);

                for (size_t i = 0; i < this->externalIds.size(); i++) {

                    deleteStmt.ResetAndUnbind();
                    deleteStmt.BindInt64(0, this->playlistId);
                    deleteStmt.BindText(1, this->externalIds[i].c_str());
                    deleteStmt.BindInt32(2, this->sortOrders[i]);
                    if (deleteStmt.Step() == db::Done) {
                        this->updated += static_cast<size_t>(db.LastModifiedRowCount());
                    }
                }
            }

            bool error = false;

            {
                db::Statement playlistTracks(
                    "SELECT track_external_id, sort_order FROM playlist_tracks "
                    "WHERE playlist_id=? ORDER BY sort_order ASC",
                    db);

                db::Statement updateStmt(
                    "UPDATE playlist_tracks "
                    "SET sort_order=? "
                    "WHERE playlist_id=? AND track_external_id=? AND sort_order=?",
                    db);

                int order = 0;

                playlistTracks.BindInt64(0, this->playlistId);
                while (playlistTracks.Step() == db::Row) {
                    updateStmt.ResetAndUnbind();
                    updateStmt.BindInt32(0, order++);
                    updateStmt.BindInt64(1, this->playlistId);
                    updateStmt.BindText(2, playlistTracks.ColumnText(0));
                    updateStmt.BindInt32(3, playlistTracks.ColumnInt32(1));
                    if (updateStmt.Step() != db::Done) {
                        error = true;
                        break;
                    }
                }
            }

            if (!error) {
                transaction.CommitAndRestart();
            }
            else {
                this->updated = 0;
            }

            if (this->updated > 0) {
                this->library->GetMessageQueue().Broadcast(
                    runtime::Message::Create(nullptr, message::PlaylistModified, playlistId));
            }

            return true;
        }

        std::string Name() override {
            return "RemoveFromPlaylistQuery";
        }

    private:
        ILibraryPtr library;
        int64_t playlistId;
        std::vector<std::string> externalIds;
        std::vector<int> sortOrders;
        size_t updated;
};

} } } }
