#include "TrackReadBackend.h"

#include <musikcore/db/SqliteExtensions.h>
#include <sqlite3.h>

namespace musik { namespace core { namespace library { namespace query {
    namespace {
        using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

        /* Each read gets its own transaction, isolated from the app's shared connection. */
        /* Inherits noncopyable and nonmovable ownership. */
        class SqliteTrackReadBackend final : public TrackReadBackend {
        public:
            explicit SqliteTrackReadBackend(std::string path) : path(std::move(path)) {}

            bool Run(const TrackSearchQuery& query, TrackSearchRows& rows) const override {
                const auto database = this->Open();
                return database && db::SqliteExtensions::Register(database.get()) == SQLITE_OK &&
                    query.Run(database.get(), rows);
            }

            bool Run(const TrackPageQuery& query, ProjectedTrackPage& page) const override {
                const auto database = this->Open();
                return database && query.Run(database.get(), page);
            }

        private:
            Database Open() const {
                sqlite3* raw = nullptr;
                const int opened = sqlite3_open_v2(this->path.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr);
                Database database(raw, sqlite3_close);
                if (opened != SQLITE_OK) database.reset();
                return database;
            }

            const std::string path;
        };
    }

    std::shared_ptr<const TrackReadBackend> OpenTrackReadBackend(std::string databasePath) {
        return std::make_shared<SqliteTrackReadBackend>(std::move(databasePath));
    }

} } } }
