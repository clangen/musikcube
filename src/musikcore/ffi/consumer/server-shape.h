#pragma once

#include <musikcore/server.h>
#include "../common/primitives.h"

namespace musik { namespace core { namespace ffi {
    inline bool valid_server_configuration(const mc_server_configuration* configuration) {
        return configuration && configuration->metadata_port && configuration->audio_port &&
            configuration->cache_count >= 0 && configuration->active_transcode_limit >= 0 &&
            valid_text(configuration->password);
    }
} } }
