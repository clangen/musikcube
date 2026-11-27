#include "TrackReadBackend.h"
#include <musikcore/db/ScopedQueryCancellation.h>

#include <musikcore/db/SqliteExtensions.h>
#include <sqlite3.h>
#include <chrono>
#include <mutex>

namespace musik { namespace core { namespace library { namespace query {
    namespace {
        using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

        /* Reuse a private reader while each statement or read transaction takes a fresh snapshot. */
        /* Inherits noncopyable and nonmovable ownership. */
        class SqliteTrackReadBackend final : public TrackReadBackend {
        public:
            explicit SqliteTrackReadBackend(std::string path) : path(std::move(path)) {}

            bool Run(const TrackSearchQuery& query, TrackSearchRows& rows,
                const std::atomic_bool& canceled) const override
            {
                return this->Read(query, rows, canceled);
            }

            bool Run(const TrackPageQuery& query, ProjectedTrackPage& page,
                const std::atomic_bool& canceled) const override
            {
                return this->Read(query, page, canceled);
            }

        private:
            template<class Query, class Result>
            bool Read(const Query& query, Result& result, const std::atomic_bool& canceled) const {
                std::lock_guard<std::mutex> lock(this->mutex);
                if (canceled.load() || (!this->database && !this->Open(canceled))) {
                    return false;
                }
                db::ScopedQueryCancellation control { this->database.get(), canceled };
                return query.Run(this->database.get(), result);
            }

            bool Open(const std::atomic_bool& canceled) const {
                sqlite3* raw = nullptr;
                const int opened = sqlite3_open_v2(this->path.c_str(), &raw, SQLITE_OPEN_READONLY | SQLITE_OPEN_PRIVATECACHE, nullptr);
                Database database(raw, sqlite3_close);
                if (opened != SQLITE_OK) {
                    return false;
                }
                db::ScopedQueryCancellation control { database.get(), canceled };
                /* keep metadata pages warm without retaining the full search working set. */
                if (sqlite3_exec(database.get(), "PRAGMA cache_size=-256", nullptr, nullptr, nullptr) != SQLITE_OK ||
                    db::SqliteExtensions::Register(database.get()) != SQLITE_OK)
                {
                    return false;
                }
                this->database = std::move(database);
                return true;
            }

            const std::string path;
            mutable std::mutex mutex;
            mutable Database database { nullptr, sqlite3_close };
        };
    }

    std::shared_ptr<const TrackReadBackend> OpenTrackReadBackend(std::string databasePath) {
        return std::make_shared<SqliteTrackReadBackend>(std::move(databasePath));
    }

} } } }
