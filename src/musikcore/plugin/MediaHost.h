#pragma once

#include <common/DeleteDefaults.h>
#include <musikcore/ffi/plugin_abi/PluginRegistry.h>
#include <musikcore/plugin/media.h>
#include <musikcore/plugin/settings.h>

namespace musik { namespace core { namespace plugin {

    /* one session's immutable media providers. playback and server workers
    can probe concurrently, including a subsong provider opening its parent.
    the session stops those workers before destroying this host. */
    class MediaHost {
    public:
        MUSIK_DELETE_COPY_AND_MOVE(MediaHost);
        explicit MediaHost(const ffi::PluginRegistry& registry);

        const mc_media_host_api& Api() const { return this->api; }

    private:
        struct Provider {
            ffi::PluginRegistry::Factory<mc_stream_factory> streams;
            ffi::PluginRegistry::Factory<mc_decoder_factory> decoders;
            mc_preferences_host_api preferences;
            mc_plugin_services services;
        };

        static mc_result MC_CALL CreateStream(void*, mc_string_view, mc_stream*, mc_error**);
        static mc_result MC_CALL CreateDecoder(void*, const mc_stream*, mc_decoder*, mc_error**);

        std::string dataDirectory;
        mc_media_host_api api;
        std::vector<Provider> providers;
    };

} } }
