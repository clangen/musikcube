#include <musikcore/plugin.h>
#include <musikcore/plugin/indexer.h>

#include <condition_variable>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>

namespace {
    struct Source {
        const mc_host_api* host;
        std::mutex mutex;
        std::condition_variable changed;
        bool interrupted = false;
    };

    void log(Source& source, const char* message) {
        source.host->log(source.host->host_context, MC_LOG_INFO, { "index-gate", 10 },
            { message, std::strlen(message) });
    }

    mc_result MC_CALL create(void* context, const mc_plugin_services* services, void** output, mc_error**) {
        const char* mode = std::getenv("MUSIKCORE_TEST_INDEX_MODE");
        if (mode && std::strcmp(mode, "create-fail") == 0) {
            /* requests made before creation fails must not retain a dead source. */
            services->indexer->request_rescan(services->indexer->context);
            return MC_ERROR_IO;
        }
        *output = new (std::nothrow) Source { static_cast<const mc_host_api*>(context) };
        return *output ? MC_OK : MC_ERROR_INTERNAL;
    }

    void MC_CALL destroy(void* instance) { delete static_cast<Source*>(instance); }

    mc_result MC_CALL info(void*, mc_indexer_source_info* output) {
        output->source_key = { "index-gate", 10 };
        output->stable_ids = 1;
        output->validate_existing_tracks = 0;
        return MC_OK;
    }

    mc_result MC_CALL begin(void* instance, mc_error**) {
        auto& source = *static_cast<Source*>(instance);
        std::lock_guard lock(source.mutex);
        source.interrupted = false;
        return MC_OK;
    }

    /* each record has enough metadata to be visible through the real catalog. */
    mc_result save(const mc_indexer_writer& writer, const char* id) {
        mc_metadata_record* record = nullptr;
        auto result = writer.create_record(writer.context, &record);
        if (result != MC_OK) {
            return result;
        }
        mc_metadata_writer metadata = MC_METADATA_WRITER_DEFAULT;
        result = writer.get_writer(writer.context, record, &metadata);
        if (result == MC_OK) {
            const mc_string_view value { id, std::strlen(id) };
            result = metadata.set_value(metadata.context, { "filename", 8 }, value);
            if (result == MC_OK) {
                result = metadata.set_value(metadata.context, { "title", 5 }, value);
            }
            if (result == MC_OK) {
                result = metadata.set_value(metadata.context, { "filetime", 8 }, { "-1", 2 });
            }
            if (result == MC_OK) {
                mc_replay_gain_values gain = MC_REPLAY_GAIN_VALUES_DEFAULT;
                gain.track_gain = -6.0f;
                result = metadata.set_replay_gain(metadata.context, &gain);
            }
            if (result == MC_OK) {
                result = writer.save(writer.context, record, value);
            }
        }
        writer.destroy_record(writer.context, &record);
        return result;
    }

    mc_result MC_CALL scan(void* instance, const mc_string_view*, size_t, const mc_indexer_writer* writer,
        mc_indexer_scan_decision* decision, mc_error**)
    {
        auto& source = *static_cast<Source*>(instance);
        /* the host must reject a foreign record before touching its contents. */
        if (writer->save(writer->context, reinterpret_cast<const mc_metadata_record*>(1), { "bad", 3 }) !=
            MC_ERROR_INVALID_ARGUMENT)
        {
            return MC_ERROR_PLUGIN;
        }
        if (writer->checkpoint(writer->context, (uint64_t) MC_INDEXER_PROGRESS_MAX + 1) !=
            MC_ERROR_INVALID_ARGUMENT)
        {
            return MC_ERROR_PLUGIN;
        }
        auto result = save(*writer, "committed");
        int64_t modified = 0;
        if (result == MC_OK && (writer->last_modified(writer->context, { "committed", 9 }, &modified) != MC_OK ||
            modified != -1))
        {
            return MC_ERROR_PLUGIN;
        }
        if (result == MC_OK) {
            result = writer->checkpoint(writer->context, 1);
        }
        if (result == MC_OK) {
            result = save(*writer, "pending");
        }
        const char* mode = std::getenv("MUSIKCORE_TEST_INDEX_MODE");
        if (result != MC_OK && mode && std::strcmp(mode, "ignore") == 0) {
            /* even a plugin that overlooks a failed write cannot commit it. */
            *decision = MC_INDEXER_SCAN_COMMIT;
            return MC_OK;
        }
        if (result != MC_OK) {
            return result;
        }
        if (mode && std::strcmp(mode, "cancel") == 0) {
            log(source, "waiting");
            std::unique_lock lock(source.mutex);
            if (!source.changed.wait_for(lock, std::chrono::seconds(10), [&] { return source.interrupted; })) {
                return MC_ERROR_PLUGIN;
            }
            *decision = MC_INDEXER_SCAN_COMMIT;
            return MC_OK;
        }
        return MC_ERROR_PLUGIN;
    }

    mc_result MC_CALL validate(void*, mc_string_view, const mc_indexer_writer*, mc_error**) {
        return MC_OK;
    }

    mc_result MC_CALL end(void* instance, mc_indexer_scan_decision decision, mc_error**) {
        log(*static_cast<Source*>(instance), decision == MC_INDEXER_SCAN_ROLLBACK ? "rolled back" : "unexpected commit");
        return MC_OK;
    }

    void MC_CALL interrupt(void* instance) {
        auto& source = *static_cast<Source*>(instance);
        std::lock_guard lock(source.mutex);
        source.interrupted = true;
        source.changed.notify_all();
    }

    mc_result MC_CALL initialize(const mc_host_api* host, void**, mc_error**) {
        static const mc_indexer_source_vtable operations {
            .struct_size = sizeof(mc_indexer_source_vtable),
            .destroy = destroy,
            .get_info = info,
            .begin_scan = begin,
            .scan = scan,
            .validate_track = validate,
            .end_scan = end,
            .interrupt = interrupt,
        };
        /* the fixture has one session, and registration owns no resources. */
        static mc_indexer_source_factory factory = MC_INDEXER_SOURCE_FACTORY_DEFAULT;
        factory.context = const_cast<mc_host_api*>(host);
        factory.instance_api = &operations;
        factory.create = create;
        mc_plugin_capability capability = MC_PLUGIN_CAPABILITY_DEFAULT;
        capability.id = { MC_PLUGIN_CAPABILITY_INDEXER_SOURCE, sizeof(MC_PLUGIN_CAPABILITY_INDEXER_SOURCE) - 1 };
        capability.api = &factory;
        capability.api_size = sizeof(factory);
        return host->register_plugin_capability(host->host_context, &capability);
    }

    void MC_CALL shutdown(void*) {}
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "org.musikcube.test.index-gate", 29 };
    plugin->name = { "Index gate fixture", 18 };
    plugin->version = { "1", 1 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
