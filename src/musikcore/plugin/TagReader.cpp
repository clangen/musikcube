#include "TagReader.h"

#include "MetadataWriter.h"
#include "Plugins.h"
#include <common/Contract.h>
#include <musikcore/support/Common.h>

namespace musik { namespace core { namespace plugin {

    TagReader::TagReader(ffi::PluginRegistry::Factory<mc_tag_reader_factory> factory, const std::string& name)
    : factory(std::move(factory))
    , preferences(ReadSettingsService(name))
    , services MC_PLUGIN_SERVICES_DEFAULT
    , dataDirectory(GetDataDirectory()) {
        this->services.preferences = &this->preferences;
        this->services.data_directory = { this->dataDirectory.data(), this->dataDirectory.size() };
        this->services.media = &Media();
    }

    bool TagReader::CanRead(const std::string& extension) const {
        uint32_t supported = 0;
        const auto status = this->factory.Invoke([&](const auto& table) {
            return table.can_read(table.context, { extension.data(), extension.size() }, &supported);
        });
        return status == MC_OK && supported;
    }

    bool TagReader::Read(const std::string& uri, sdk::ITagStore& store) const {
        const auto writer = CreateMetadataWriter(store);
        return this->factory.Invoke([&](const auto& table) {
            void* instance = nullptr;
            const auto created = table.create(table.context, &this->services, &instance, nullptr);
            musik::contract::Assert(created == MC_OK || !instance, "failed tag reader factory returned an instance");
            if (created != MC_OK) {
                return created;
            }
            musik::contract::Assert(instance, "tag reader factory returned no instance");
            const auto result = table.instance_api->read(instance, { uri.data(), uri.size() }, &writer, nullptr);
            table.instance_api->destroy(instance);
            return result;
        }) == MC_OK;
    }

} } }
