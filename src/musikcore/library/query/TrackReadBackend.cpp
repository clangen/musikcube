#include "TrackReadBackend.h"

#include <musikcore/db/SqliteExtensions.h>
#include <sqlite3.h>
#include <chrono>

namespace musik { namespace core { namespace library { namespace query {
    namespace {
        constexpr int readBusyTimeoutMs = 10000;
        constexpr int readBusyRetryMs = 10;
        constexpr int readProgressInterval = 1000;

        using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

        struct ReadControl {
            explicit ReadControl(const std::atomic_bool& canceled) : canceled(canceled) {}

            const std::atomic_bool& canceled;
            std::chrono::steady_clock::time_point busyDeadline {};

            static int WaitForLock(void* data, int attempts) noexcept {
                auto& control = *static_cast<ReadControl*>(data);
                const auto now = std::chrono::steady_clock::now();
                if (!attempts) {
                    control.busyDeadline = now + std::chrono::milliseconds(readBusyTimeoutMs);
                }
                if (control.canceled.load() || now >= control.busyDeadline) {
                    return 0;
                }
                sqlite3_sleep(readBusyRetryMs);
                return !control.canceled.load();
            }

            static int CheckProgress(void* data) noexcept {
                return static_cast<ReadControl*>(data)->canceled.load();
            }
        };

        /* Each read gets its own transaction, isolated from the app's shared connection. */
        /* Inherits noncopyable and nonmovable ownership. */
        class SqliteTrackReadBackend final : public TrackReadBackend {
        public:
            explicit SqliteTrackReadBackend(std::string path) : path(std::move(path)) {}

            bool Run(const TrackSearchQuery& query, TrackSearchRows& rows,
                const std::atomic_bool& canceled) const override
            {
                ReadControl control { canceled };
                const auto database = this->Open(control);
                return database && db::SqliteExtensions::Register(database.get()) == SQLITE_OK &&
                    query.Run(database.get(), rows);
            }

            bool Run(const TrackPageQuery& query, ProjectedTrackPage& page,
                const std::atomic_bool& canceled) const override
            {
                ReadControl control { canceled };
                const auto database = this->Open(control);
                return database && query.Run(database.get(), page);
            }

        private:
            Database Open(ReadControl& control) const {
                if (control.canceled.load()) {
                    return { nullptr, sqlite3_close };
                }
                sqlite3* raw = nullptr;
                const int opened = sqlite3_open_v2(this->path.c_str(), &raw, SQLITE_OPEN_READONLY | SQLITE_OPEN_PRIVATECACHE, nullptr);
                Database database(raw, sqlite3_close);
                if (opened != SQLITE_OK) {
                    database.reset();
                }
                else {
                    /* keep lock retries bounded and let shutdown interrupt the wait. */
                    sqlite3_busy_handler(database.get(), ReadControl::WaitForLock, &control);
                    sqlite3_progress_handler(database.get(), readProgressInterval, ReadControl::CheckProgress, &control);
                }
                return database;
            }

            const std::string path;
        };
    }

    std::shared_ptr<const TrackReadBackend> OpenTrackReadBackend(std::string databasePath) {
        return std::make_shared<SqliteTrackReadBackend>(std::move(databasePath));
    }

} } } }
