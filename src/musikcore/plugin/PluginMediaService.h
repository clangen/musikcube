#pragma once

#include <common/DeleteDefaults.h>
#include <musikcore/ffi/plugin_abi/PluginRegistry.h>
#include <musikcore/plugin/media.h>
#include <musikcore/plugin/settings.h>

namespace musik { namespace core { namespace audio { class Dsp; } } }

namespace musik { namespace core { namespace plugin {

    /* one session's immutable media providers. playback and server workers
    can probe concurrently, including a subsong provider opening its parent.
    the session stops those workers before destroying this host. */
    class PluginMediaService {
    public:
        MUSIK_DELETE_COPY_AND_MOVE(PluginMediaService);
        explicit PluginMediaService(const ffi::PluginRegistry& registry);

        const mc_plugin_media_service& Api() const { return this->api; }

        std::vector<std::shared_ptr<audio::Dsp>> CreateDsps() const;

    private:
        struct Provider {
            ffi::PluginRegistry::Factory<mc_stream_factory> streams;
            ffi::PluginRegistry::Factory<mc_decoder_factory> decoders;
            ffi::PluginRegistry::Factory<mc_encoder_factory> encoders;
            ffi::PluginRegistry::Factory<mc_dsp_factory> dsp;
            mc_plugin_preferences_service preferences;
            mc_plugin_services services;
        };

        static mc_result MC_CALL CreateStream(void*, mc_string_view, mc_stream*, mc_error**);
        static mc_result MC_CALL CreateDecoder(void*, const mc_stream*, mc_decoder*, mc_error**);
        static mc_result MC_CALL CreateEncoder(void*, mc_string_view, mc_encoder*, mc_error**);

        std::string dataDirectory;
        mc_plugin_media_service api;
        std::vector<Provider> providers;
    };

} } }
