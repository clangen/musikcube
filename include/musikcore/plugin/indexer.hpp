#ifndef MUSIKCORE_PLUGIN_INDEXER_HPP
#define MUSIKCORE_PLUGIN_INDEXER_HPP

#include <musikcore/plugin/indexer.h>
#include <musikcore/plugin.h>

#include <charconv>
#include <cstring>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace musik { namespace core { namespace plugin {

    /* encode the container path and subtrack number in a source-specific URI. */
    inline std::string CreateExternalId(const std::string& prefix, const std::string& path, int track) {
        return prefix + "://" + std::to_string(track) + "/" + path;
    }

    /* recover the container path and subtrack number from a previously saved URI. */
    inline bool ParseExternalId(const std::string& prefix, const std::string& id, std::string& path, int& track) {
        const auto beginning = prefix + "://";
        if (prefix.empty() || id.compare(0, beginning.size(), beginning) != 0) {
            return false;
        }
        const auto slash = id.find('/', beginning.size());
        if (slash == std::string::npos || slash == beginning.size() || slash + 1 == id.size()) {
            return false;
        }
        int parsed = 0;
        const auto* first = id.data() + beginning.size();
        const auto* end = id.data() + slash;
        const auto result = std::from_chars(first, end, parsed);
        if (result.ec != std::errc() || result.ptr != end || parsed < 0) {
            return false;
        }
        path.assign(id, slash + 1, std::string::npos);
        track = parsed;
        return true;
    }

    /* builds a source-owned track during a scan and releases it automatically.
    use the writer supplied to the current source callback. records cannot be
    copied or moved and must be destroyed before that callback returns. */
    class IndexerRecord {
    public:
        explicit IndexerRecord(const mc_indexer_writer& writer) : writer(writer) {
            if (writer.create_record(writer.context, &this->record) != MC_OK || !this->record ||
                writer.get_writer(writer.context, this->record, &this->metadata) != MC_OK)
            {
                if (this->record) {
                    writer.destroy_record(writer.context, &this->record);
                }
                throw std::runtime_error("could not create indexer record");
            }
        }

        IndexerRecord(const IndexerRecord& other) = delete;
        IndexerRecord& operator=(const IndexerRecord& other) = delete;
        IndexerRecord(IndexerRecord&& other) = delete;
        IndexerRecord& operator=(IndexerRecord&& other) = delete;

        ~IndexerRecord() {
            this->writer.destroy_record(this->writer.context, &this->record);
        }

        void SetValue(const char* key, const char* value) {
            if (value && *value && this->metadata.set_value(this->metadata.context,
                { key, std::strlen(key) }, { value, std::strlen(value) }) != MC_OK)
            {
                throw std::runtime_error("could not write index metadata");
            }
        }

        const mc_metadata_record* Get() const { return this->record; }

    private:
        const mc_indexer_writer& writer;
        mc_metadata_record* record { nullptr };
        mc_metadata_writer metadata = MC_METADATA_WRITER_DEFAULT;
    };

    /* applies source-scoped changes to the library while scanning. errors
    throw so the plugin can report a failed scan and discard its current batch. */
    class IndexerWriter {
    public:
        explicit IndexerWriter(const mc_indexer_writer& writer) : writer(writer) {}

        void Save(const IndexerRecord& record, const std::string& externalId) const {
            Check(this->writer.save(this->writer.context, record.Get(), { externalId.data(), externalId.size() }));
        }

        void RemoveByExternalId(const char* id) const {
            Check(this->writer.remove_external_id(this->writer.context, { id, std::strlen(id) }));
        }

        std::optional<int64_t> GetLastModifiedTime(const std::string& externalId) const {
            int64_t modified = 0;
            const auto result = this->writer.last_modified(this->writer.context,
                { externalId.data(), externalId.size() }, &modified);
            if (result == MC_ERROR_NOT_FOUND) {
                return std::nullopt;
            }
            Check(result);
            return modified;
        }

        void CommitProgress(unsigned updated) const {
            Check(this->writer.checkpoint(this->writer.context, updated));
        }

        const mc_indexer_writer& Get() const { return this->writer; }

    private:
        static void Check(mc_result result) {
            if (result != MC_OK) {
                throw std::runtime_error("indexer writer operation failed");
            }
        }

        const mc_indexer_writer& writer;
    };

    /* reads a source's saved options and sends diagnostics through its host. */
    class IndexerServices {
    public:
        IndexerServices(const mc_host_api& host, const mc_plugin_services& services)
        : host(host), settings(*services.settings) {}

        bool Boolean(const char* key, bool fallback) const {
            bool value = fallback;
            if (this->settings.read_boolean(this->settings.context,
                { key, std::strlen(key) }, fallback, &value) != MC_OK)
            {
                throw std::runtime_error("could not read indexer setting");
            }
            return value;
        }

        double Number(const char* key, double fallback) const {
            double value = fallback;
            if (this->settings.read_number(this->settings.context,
                { key, std::strlen(key) }, fallback, &value) != MC_OK)
            {
                throw std::runtime_error("could not read indexer setting");
            }
            return value;
        }

        std::string Text(const char* key, const char* fallback) const {
            struct Result {
                std::string text;
                bool failed = false;
            } result;
            const auto read = [](void* user, mc_string_view text) {
                auto& result = *static_cast<Result*>(user);
                try {
                    result.text.assign(text.data ? text.data : "", text.length);
                }
                catch (...) {
                    result.failed = true;
                }
            };
            if (this->settings.read_text(this->settings.context, { key, std::strlen(key) },
                { fallback, std::strlen(fallback) }, read, &result) != MC_OK || result.failed)
            {
                throw std::runtime_error("could not read indexer setting");
            }
            return result.text;
        }

        void Error(const char* tag, const std::string& message) const {
            this->host.log(this->host.host_context, MC_LOG_ERROR, { tag, std::strlen(tag) },
                { message.data(), message.size() });
        }

        void Info(const char* tag, const char* message) const {
            this->host.log(this->host.host_context, MC_LOG_INFO, { tag, std::strlen(tag) },
                { message, std::strlen(message) });
        }

    private:
        const mc_host_api& host;
        const mc_plugin_settings_service& settings;
    };

    /* converts a compatible C++ source into C callbacks at compile time,
    without requiring a virtual base class or runtime polymorphism. */
    template<typename Source>
    class IndexerSourceAdapter {
    public:
        static mc_result Register(const mc_host_api& host) {
            static const mc_indexer_source_factory factory {
                sizeof(mc_indexer_source_factory), nullptr, &Operations(), Create,
            };
            mc_plugin_capability capability = MC_PLUGIN_CAPABILITY_DEFAULT;
            capability.id = { MC_PLUGIN_CAPABILITY_INDEXER_SOURCE, sizeof(MC_PLUGIN_CAPABILITY_INDEXER_SOURCE) - 1 };
            capability.api = &factory;
            capability.api_size = sizeof(factory);
            return host.register_plugin_capability(host.host_context, &capability);
        }

    private:
        static const mc_indexer_source_vtable& Operations() {
            static const mc_indexer_source_vtable operations {
                sizeof(mc_indexer_source_vtable), Destroy, Info, Begin, Scan, Validate, End, Interrupt,
            };
            return operations;
        }

        static mc_result MC_CALL Create(void*, const mc_plugin_services* services,
            void** output, mc_error**) noexcept
        {
            *output = nullptr;
            try {
                *output = new Source(*services->host, *services);
                return MC_OK;
            }
            catch (...) {
                return MC_ERROR_PLUGIN;
            }
        }

        static void MC_CALL Destroy(void* instance) noexcept {
            delete static_cast<Source*>(instance);
        }

        static mc_result MC_CALL Info(void* instance, mc_indexer_source_info* info) noexcept {
            if (!info || info->struct_size < sizeof(*info)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            try {
                static_assert(std::is_lvalue_reference_v<decltype(Source::Key())>,
                    "IndexerSource key must refer to plugin-owned storage");
                static_assert(std::is_lvalue_reference_v<decltype(Source::Label())>,
                    "IndexerSource label must refer to plugin-owned storage");
                const auto& key = Source::Key();
                const auto& label = Source::Label();
                auto& source = *static_cast<Source*>(instance);
                info->source_key = { key.data(), key.size() };
                info->stable_ids = source.HasStableIds();
                info->validate_existing_tracks = source.NeedsTrackScan();
                info->display_name = { label.data(), label.size() };
                return MC_OK;
            }
            catch (...) {
                return MC_ERROR_PLUGIN;
            }
        }

        static mc_result MC_CALL Begin(void* instance, mc_error**) noexcept {
            try {
                static_cast<Source*>(instance)->OnBeforeScan();
                return MC_OK;
            }
            catch (...) {
                return MC_ERROR_PLUGIN;
            }
        }

        static mc_result MC_CALL Scan(void* instance, const mc_string_view* paths, size_t count,
            const mc_indexer_writer* writer, mc_indexer_scan_decision* decision, mc_error**) noexcept
        {
            try {
                std::vector<std::string> storage;
                std::vector<const char*> pointers;
                storage.reserve(count);
                pointers.reserve(count);
                for (size_t i = 0; i < count; ++i) {
                    storage.emplace_back(paths[i].data, paths[i].length);
                }
                for (const auto& path : storage) {
                    pointers.push_back(path.c_str());
                }
                IndexerWriter output(*writer);
                *decision = static_cast<Source*>(instance)->Scan(&output, pointers.data(), pointers.size())
                    ? MC_INDEXER_SCAN_COMMIT : MC_INDEXER_SCAN_ROLLBACK;
                return MC_OK;
            }
            catch (...) {
                return MC_ERROR_PLUGIN;
            }
        }

        static mc_result MC_CALL Validate(void* instance, mc_string_view id, int64_t lastModified, const mc_indexer_writer* writer,
            mc_error**) noexcept
        {
            try {
                const std::string text(id.data, id.length);
                IndexerWriter output(*writer);
                static_cast<Source*>(instance)->ScanTrack(&output, text.c_str(), lastModified);
                return MC_OK;
            }
            catch (...) {
                return MC_ERROR_PLUGIN;
            }
        }

        static mc_result MC_CALL End(void* instance, mc_indexer_scan_decision, mc_error**) noexcept {
            try {
                static_cast<Source*>(instance)->OnAfterScan();
                return MC_OK;
            }
            catch (...) {
                return MC_ERROR_PLUGIN;
            }
        }

        static void MC_CALL Interrupt(void* instance) noexcept {
            static_cast<Source*>(instance)->Interrupt();
        }

    };

    /* register a C++ source without exposing its adapter or factory lifetime. */
    template<typename Source> mc_result RegisterIndexerSource(const mc_host_api& host) {
        return IndexerSourceAdapter<Source>::Register(host);
    }

} } }

#endif
