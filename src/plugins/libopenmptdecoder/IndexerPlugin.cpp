#include "OpenMptIndexerSource.h"
#include <musikcore/plugin.h>
#include <musikcore/plugin/registration.hpp>

const mc_configuration_api& OpenMptIndexerConfiguration() {
    static const mc_text_setting fields[] {
        { { "default_album_name", 18 }, { "[unknown %s album]", 18 } },
        { { "default_artist_name", 19 }, { "[unknown %s artist]", 19 } },
    };
    static const mc_configuration_api schema {
        .struct_size = sizeof(mc_configuration_api),
        .texts = fields,
        .text_count = sizeof(fields) / sizeof(*fields),
    };
    return schema;
}
