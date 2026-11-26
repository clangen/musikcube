#include "IndexerSource.h"

#include "MetadataWriter.h"
#include "Plugins.h"
#include <musikcore/library/Indexer.h>
#include <musikcore/library/track/IndexerTrack.h>
#include <musikcore/library/IndexerSourceId.h>
#include <common/Contract.h>
#include <musikcore/ffi/common/error.h>
#include <musikcore/ffi/common/primitives.h>
#include <musikcore/support/Common.h>
#include <utf8/utf8.h>
#include <limits>
#include <cstring>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace musik { namespace core { namespace plugin {

    namespace {
        /* a scan owns its records even if a plugin forgets to release them. */
        class IndexerWriterScope {
        public:
            IndexerWriterScope(Indexer& writer, IndexerSource& source)
            : writer(writer), source(source) {}

            ~IndexerWriterScope() {
                for (auto* record : this->records) {
                    delete record;
                }
            }

            mc_indexer_writer Table() {
                return {
                    .struct_size = sizeof(mc_indexer_writer),
                    .context = this,
                    .create_record = CreateRecord,
                    .get_writer = GetWriter,
                    .destroy_record = DestroyRecord,
                    .save = Save,
                    .remove_uri = RemoveUri,
                    .remove_external_id = RemoveExternalId,
                    .remove_all = RemoveAll,
                    .last_modified = LastModified,
                    .checkpoint = Checkpoint,
                };
            }

            bool Failed() const { return this->failed; }

        private:
            mc_result Observe(mc_result result) {
                if (result != MC_OK && result != MC_ERROR_NOT_FOUND && result != MC_ERROR_INVALID_ARGUMENT) {
                    this->failed = true;
                }
                return result;
            }

            static IndexerTrack* Store(mc_metadata_record* record) {
                return reinterpret_cast<IndexerTrack*>(record);
            }

            static mc_result MC_CALL CreateRecord(void* context, mc_metadata_record** output) {
                if (!output || *output) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                auto& self = *static_cast<IndexerWriterScope*>(context);
                return self.Observe(ffi::exception_barrier(nullptr, [&] {
                    auto record = self.writer.CreateWriter();
                    if (!record) {
                        return MC_ERROR_INTERNAL;
                    }
                    auto* raw = record.get();
                    self.records.insert(raw);
                    record.release();
                    *output = reinterpret_cast<mc_metadata_record*>(raw);
                    return MC_OK;
                }));
            }

            static mc_result MC_CALL GetWriter(void* context, mc_metadata_record* record, mc_metadata_writer* output) {
                auto& self = *static_cast<IndexerWriterScope*>(context);
                auto* store = Store(record);
                if (!output || !self.records.count(store)) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                *output = CreateMetadataWriter(*store);
                return MC_OK;
            }

            static void MC_CALL DestroyRecord(void* context, mc_metadata_record** record) {
                if (!record || !*record) {
                    return;
                }
                auto& self = *static_cast<IndexerWriterScope*>(context);
                auto* store = Store(*record);
                musik::contract::Assert(self.records.erase(store) == 1, "destroying a foreign indexer record");
                delete store;
                *record = nullptr;
            }

            static mc_result MC_CALL Save(void* context, const mc_metadata_record* record, mc_string_view externalId) {
                auto& self = *static_cast<IndexerWriterScope*>(context);
                auto* store = Store(const_cast<mc_metadata_record*>(record));
                if (!self.records.count(store) || !ffi::valid_text(externalId, false)) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                return self.Observe(ffi::exception_barrier(nullptr, [&] {
                    const std::string id(externalId.data, externalId.length);
                    return self.writer.Save(&self.source, store, id.c_str()) ? MC_OK : MC_ERROR_IO;
                }));
            }

            static mc_result MC_CALL RemoveUri(void* context, mc_string_view uri) {
                if (!ffi::valid_text(uri, false)) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                auto& self = *static_cast<IndexerWriterScope*>(context);
                return self.Observe(ffi::exception_barrier(nullptr, [&] {
                    const std::string text(uri.data, uri.length);
                    return self.writer.RemoveByUri(&self.source, text.c_str()) ? MC_OK : MC_ERROR_IO;
                }));
            }

            static mc_result MC_CALL RemoveExternalId(void* context, mc_string_view id) {
                if (!ffi::valid_text(id, false)) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                auto& self = *static_cast<IndexerWriterScope*>(context);
                return self.Observe(ffi::exception_barrier(nullptr, [&] {
                    const std::string text(id.data, id.length);
                    return self.writer.RemoveByExternalId(&self.source, text.c_str()) ? MC_OK : MC_ERROR_IO;
                }));
            }

            static mc_result MC_CALL RemoveAll(void* context, uint64_t* removed) {
                if (!removed) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                auto& self = *static_cast<IndexerWriterScope*>(context);
                return self.Observe(ffi::exception_barrier(nullptr, [&] {
                    const auto count = self.writer.RemoveAll(&self.source);
                    if (count < 0) {
                        return MC_ERROR_IO;
                    }
                    *removed = static_cast<uint64_t>(count);
                    return MC_OK;
                }));
            }

            static mc_result MC_CALL LastModified(void* context, mc_string_view id, int64_t* modified) {
                if (!modified || !ffi::valid_text(id, false)) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                auto& self = *static_cast<IndexerWriterScope*>(context);
                return self.Observe(ffi::exception_barrier(nullptr, [&] {
                    const std::string text(id.data, id.length);
                    return self.writer.GetLastModifiedTime(&self.source, text.c_str(), *modified)
                        ? MC_OK : MC_ERROR_NOT_FOUND;
                }));
            }

            static mc_result MC_CALL Checkpoint(void* context, uint64_t updated) {
                if (updated > MC_INDEXER_PROGRESS_MAX) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                auto& self = *static_cast<IndexerWriterScope*>(context);
                if (self.failed) {
                    return MC_ERROR_IO;
                }
                return self.Observe(ffi::exception_barrier(nullptr, [&] {
                    self.writer.CommitProgress(&self.source, static_cast<unsigned>(updated));
                    return MC_OK;
                }));
            }

            Indexer& writer;
            IndexerSource& source;
            std::unordered_set<IndexerTrack*> records;
            bool failed { false };
        };
    }

    IndexerSource::IndexerSource(ffi::PluginRegistry::Factory<mc_indexer_source_factory> factory,
        const std::string& pluginId, const std::string& name, Indexer& indexer)
    : factory(std::move(factory))
    , indexer(indexer)
    , preferences(ReadSettingsService(name))
    , services MC_PLUGIN_SERVICES_DEFAULT
    , sourceHost MC_PLUGIN_INDEXER_SERVICE_DEFAULT
    , dataDirectory(GetDataDirectory())
    , pluginId(pluginId) {
        /* the source may retain these services until its instance is destroyed. */
        this->services.preferences = &this->preferences;
        this->services.data_directory = { this->dataDirectory.data(), this->dataDirectory.size() };
        this->services.media = &Media();

        /* let the source request a rescan when its external data changes. */
        this->sourceHost.context = this;
        this->sourceHost.request_rescan = [](void* context) -> mc_result {
            return ffi::exception_barrier(nullptr, [&] {
                auto& self = *static_cast<IndexerSource*>(context);
                return self.indexer.ScheduleRescan(&self) ? MC_OK : MC_ERROR_CANCELLED;
            });
        };

        this->services.indexer = &this->sourceHost;
        this->services.host = this->factory.Host();

        mc_result result = MC_ERROR_INTERNAL;
        try {
            result = this->factory.Invoke([&](const auto& table) -> mc_result {
                const auto status = table.create(table.context, &this->services, &this->instance, nullptr);
                if (status != MC_OK) {
                    musik::contract::Assert(!this->instance, "failed indexer source factory returned an instance");
                    return status;
                }
                musik::contract::Assert(this->instance, "indexer source factory returned no instance");

                /* validate the source's identity before using it to identify saved tracks. */
                mc_indexer_source_info info = MC_INDEXER_SOURCE_INFO_DEFAULT;
                const auto inspected = table.instance_api->get_info(this->instance, &info);
                if (inspected != MC_OK || info.struct_size < sizeof(info) ||
                    info.source_key.length > MC_PLUGIN_METADATA_MAX_BYTES || !ffi::valid_text(info.source_key, false) ||
                    !utf8::is_valid(info.source_key.data, info.source_key.data + info.source_key.length) ||
                    info.stable_ids > 1 || info.validate_existing_tracks > 1 ||
                    info.display_name.length > MC_PLUGIN_METADATA_MAX_BYTES ||
                    !ffi::valid_text(info.display_name, false) ||
                    !utf8::is_valid(info.display_name.data, info.display_name.data + info.display_name.length))
                {
                    return MC_ERROR_PLUGIN;
                }

                this->sourceKey.assign(info.source_key.data, info.source_key.length);
                this->label.assign(info.display_name.data, info.display_name.length);
                this->sourceId = library::IndexerSourceId(this->pluginId, this->sourceKey);
                this->stableIds = info.stable_ids != 0;
                this->validateTracks = info.validate_existing_tracks != 0;
                return MC_OK;
            });
        }
        catch (...) {
            /* construction failed after the plugin created an instance. */
            this->DestroyInstance();
            throw;
        }

        if (result != MC_OK) {
            this->DestroyInstance();
            throw std::runtime_error("failed to create indexer source");
        }
    }

    IndexerSource::~IndexerSource() {
        this->DestroyInstance();
    }

    void IndexerSource::DestroyInstance() noexcept {
        if (this->instance) {
            try {
                this->factory.Invoke([&](const auto& table) {
                    table.instance_api->destroy(this->instance);
                    return MC_OK;
                });
                this->instance = nullptr;
            }
            catch (...) {
                musik::contract::Assert(false, "indexer source destruction failed");
            }
        }
    }

    void IndexerSource::OnBeforeScan() {
        this->failed = false;
        this->decision = MC_INDEXER_SCAN_ROLLBACK;
        this->failed = this->factory.Invoke([&](const auto& table) {
            return table.instance_api->begin_scan(this->instance, nullptr);
        }) != MC_OK;
    }

    void IndexerSource::OnAfterScan() {
        const auto result = this->factory.Invoke([&](const auto& table) {
            return table.instance_api->end_scan(this->instance, this->failed ? MC_INDEXER_SCAN_ROLLBACK : this->decision, nullptr);
        });
        this->failed = this->failed || result != MC_OK;
    }

    musik::core::ScanResult IndexerSource::Scan(Indexer* writer, const char** paths, unsigned count) {
        if (this->failed) {
            return musik::core::ScanRollback;
        }
        std::vector<mc_string_view> views;
        views.reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            views.push_back({ paths[i], std::strlen(paths[i]) });
        }
        IndexerWriterScope scope(*writer, *this);
        const auto table = scope.Table();
        const auto result = this->factory.Invoke([&](const auto& factory) {
            return factory.instance_api->scan(this->instance, views.data(), views.size(), &table, &this->decision, nullptr);
        });
        this->failed = result != MC_OK || scope.Failed() || this->decision != MC_INDEXER_SCAN_COMMIT;
        return !this->failed && this->decision == MC_INDEXER_SCAN_COMMIT ? musik::core::ScanCommit : musik::core::ScanRollback;
    }

    void IndexerSource::ScanTrack(Indexer* writer, const char* id) {
        if (this->failed) {
            return;
        }
        IndexerWriterScope scope(*writer, *this);
        const auto table = scope.Table();
        this->failed = this->factory.Invoke([&](const auto& factory) {
            return factory.instance_api->validate_track(this->instance, { id, std::strlen(id) }, &table, nullptr);
        }) != MC_OK || scope.Failed();
    }

    bool IndexerSource::NeedsTrackScan() { return this->validateTracks; }
    void IndexerSource::Interrupt() {
        this->factory.Invoke([&](const auto& table) {
            table.instance_api->interrupt(this->instance);
            return MC_OK;
        });
    }
    bool IndexerSource::HasStableIds() { return this->stableIds; }
    int IndexerSource::SourceId() { return this->sourceId; }
    bool IndexerSource::Failed() const { return this->failed; }
    void IndexerSource::Abort() { this->failed = true; }

} } }
