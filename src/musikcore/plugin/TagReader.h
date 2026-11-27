#pragma once

#include <musikcore/ffi/plugin_abi/PluginRegistry.h>
#include <musikcore/plugin/tag.h>
#include <string>

namespace musik { namespace core { class Track; } }

namespace musik { namespace core { namespace plugin {

    /* selects and runs a plugin tag reader on indexer workers. the registry
    and services remain alive until all indexer work has stopped. */
    class TagReader {
    public:
        MUSIK_DELETE_COPY_AND_MOVE(TagReader);

        TagReader(ffi::PluginRegistry::Factory<mc_tag_reader_factory> factory, const std::string& name);

        bool CanRead(const std::string& extension) const;
        bool Read(const std::string& uri, Track& store) const;

    private:
        ffi::PluginRegistry::Factory<mc_tag_reader_factory> factory;
        mc_plugin_settings_service settings;
        mc_plugin_services services;
        std::string dataDirectory;
    };

} } }
