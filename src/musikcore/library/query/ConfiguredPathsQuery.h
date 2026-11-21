#pragma once

#include <musikcore/library/QueryBase.h>
#include <musikcore/db/Statement.h>

namespace musik { namespace core { namespace library { namespace query {

    /* reads catalog configuration without constructing an indexer. use for a
    library-only runtime; QueryBase owns synchronization and is not copyable. */
    class ConfiguredPathsQuery final : public QueryBase {
        public:
            std::string Name() override { return "ConfiguredPaths"; }
            bool GetResult() const { return this->configured; }

        private:
            bool OnRun(db::Connection& db) override {
                db::Statement statement("SELECT 1 FROM paths LIMIT 1", db);
                this->configured = statement.Step() == SQLITE_ROW;
                return true;
            }

            bool configured = false;
    };

} } } }
