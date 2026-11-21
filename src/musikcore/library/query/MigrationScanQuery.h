#pragma once

#include <musikcore/library/QueryBase.h>
#include <musikcore/db/Statement.h>

namespace musik { namespace core { namespace library { namespace query {

    /* keeps migration follow-up work with its catalog, including when no
    scanner is running. use Require inside the schema transaction and Complete
    inside the final scan transaction. QueryBase makes this noncopyable. */
    class MigrationScanQuery final : public QueryBase {
        public:
            enum class Operation {
                Read,
                Require,
                Complete
            };

            explicit MigrationScanQuery(Operation operation = Operation::Read)
            : operation(operation) {}

            std::string Name() override { return "MigrationScan"; }
            bool IsPending() const { return this->pending; }

        private:
            bool OnRun(db::Connection& db) override {
                if (this->operation == Operation::Read) {
                    db::Statement statement("SELECT 1 FROM pending_migration_scan WHERE id=1", db);
                    const auto status = statement.Step();
                    this->pending = status == db::Row;
                    return status == db::Row || status == db::Done;
                }

                const auto* sql = this->operation == Operation::Require
                    ? "INSERT OR IGNORE INTO pending_migration_scan(id) VALUES(1)"
                    : "DELETE FROM pending_migration_scan WHERE id=1";
                db::Statement statement(sql, db);
                return statement.Step() == db::Done;
            }

            Operation operation;
            bool pending = false;
    };

} } } }
