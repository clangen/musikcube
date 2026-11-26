#pragma once

#include <musikcore/library/QueryBase.h>
#include <musikcore/db/Statement.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <map>
#include <musikcore/library/LibraryBackend.h>

namespace musik { namespace core { namespace library { namespace query {

    /* catalog discovery remains independent of the active browsing scope. */
    class SourceListQuery final : public QueryBase {
    public:
        struct Entry {
            int32_t id;
            std::string label;
        };

        inline static const std::string kQueryName = "SourceListQuery";

        explicit SourceListQuery(const LibraryBackend::Shared& library = {}) {
            if (library) {
                this->loaded = library->Sources();
            }
        }


        std::string Name() override { return kQueryName; }

        const std::vector<Entry>& GetResult() const { return this->entries; }

        std::string SerializeQuery() override {
            return nlohmann::json { { "name", kQueryName }, { "options", nlohmann::json::object() } }.dump();
        }

        std::string SerializeResult() override {
            auto rows = nlohmann::json::array();
            for (const auto& entry : this->entries) {
                rows.push_back({ { "id", entry.id }, { "label", entry.label } });
            }
            return nlohmann::json { { "result", rows } }.dump();
        }

        void DeserializeResult(const std::string& data) override {
            this->SetStatus(db::IQuery::Failed);
            std::vector<Entry> rows;
            const auto response = nlohmann::json::parse(data);
            for (const auto& row : response.at("result")) {
                const auto id = row.at("id").get<int64_t>();
                const auto label = row.at("label").get<std::string>();
                if (id < 0 || id > INT32_MAX || label.empty()) {
                    throw std::invalid_argument("invalid catalog source");
                }
                rows.push_back({ static_cast<int32_t>(id), label });
            }
            this->entries = std::move(rows);
            this->SetStatus(db::IQuery::Finished);
        }

        static std::shared_ptr<SourceListQuery> DeserializeQuery(const std::string& data, const LibraryBackend::Shared& library = {}) {
            const auto request = nlohmann::json::parse(data);
            if (request.at("name") != kQueryName || !request.at("options").empty()) {
                throw std::invalid_argument("invalid source-list options");
            }
            return std::make_shared<SourceListQuery>(library);
        }

    protected:
        bool OnRun(db::Connection& database) override {
            std::map<int32_t, std::string> sources { { 0, "Music files" } };
            db::Statement statement("SELECT source_id FROM tracks WHERE source_id>=0 "
                "UNION SELECT source_id FROM playlist_tracks WHERE source_id>=0 "
                "UNION SELECT source_id FROM playlists WHERE source_id>=0", database);
            int status;
            while ((status = statement.Step()) == db::Row) {
                const auto id = statement.ColumnInt32(0);
                sources.try_emplace(id, "Source " + std::to_string(id));
            }
            if (status != db::Done) {
                return false;
            }
            for (const auto& source : this->loaded) {
                sources[source.first] = source.second;
            }
            std::vector<Entry> rows;
            for (const auto& source : sources) {
                rows.push_back({ source.first, source.second });
            }
            this->entries = std::move(rows);
            return true;
        }

    private:
        std::vector<Entry> entries;
        std::vector<std::pair<int32_t, std::string>> loaded;
    };

} } } }
