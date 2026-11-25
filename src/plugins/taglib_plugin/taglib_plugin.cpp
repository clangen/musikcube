#include <musikcore/plugin/registration.hpp>
#include "TaglibMetadataReader.h"

#include <musikcore/plugin.h>
#include <musikcore/plugin/tag.h>
#include <musikcore/sdk/version.h>

#include <new>
#include <string>

namespace {
    mc_result MC_CALL canRead(void*, mc_string_view extension, uint32_t* supported) {
        try {
            const std::string text(extension.data ? extension.data : "", extension.length);
            TaglibMetadataReader reader;
            *supported = reader.CanRead(text.c_str()) ? 1 : 0;
            return MC_OK;
        }
        catch (...) {
            return MC_ERROR_INTERNAL;
        }
    }

    mc_result MC_CALL create(void*, const mc_plugin_services*, void** instance, mc_error**) {
        *instance = nullptr;
        try {
            *instance = new (std::nothrow) TaglibMetadataReader();
            return *instance ? MC_OK : MC_ERROR_INTERNAL;
        }
        catch (...) {
            return MC_ERROR_INTERNAL;
        }
    }

    void MC_CALL destroy(void* instance) {
        delete static_cast<TaglibMetadataReader*>(instance);
    }

    mc_result MC_CALL read(void* instance, mc_string_view uri, const mc_metadata_writer* writer, mc_error**) {
        try {
            const std::string path(uri.data, uri.length);
            MetadataWriter target(*writer);
            if (!static_cast<TaglibMetadataReader*>(instance)->Read(path.c_str(), &target)) {
                return MC_ERROR_PLUGIN;
            }
            return target.Status();
        }
        catch (...) {
            return MC_ERROR_INTERNAL;
        }
    }

    mc_result MC_CALL initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        static const mc_tag_reader_vtable methods {
            .struct_size = sizeof(mc_tag_reader_vtable),
            .destroy = destroy,
            .read = read,
        };
        static const mc_tag_reader_factory factory {
            .struct_size = sizeof(mc_tag_reader_factory),
            .context = nullptr,
            .instance_api = &methods,
            .can_read = canRead,
            .create = create,
        };
        return musik::core::plugin::RegisterTagReader(*host, factory);
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
    plugin->id = { "c77ea7a4-5d57-4f17-8521-aba8eeadeda7", 36 };
    plugin->name = { "TagLib metadata reader", sizeof("TagLib metadata reader") - 1 };
    plugin->author = { "Daniel Önnerby, clangen", sizeof("Daniel Önnerby, clangen") - 1 };
    plugin->version = { MUSIKCUBE_VERSION_WITH_COMMIT_HASH, sizeof(MUSIKCUBE_VERSION_WITH_COMMIT_HASH) - 1 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
