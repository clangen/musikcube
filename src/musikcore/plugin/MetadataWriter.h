#pragma once

#include <musikcore/plugin/metadata.h>

namespace musik { namespace core { class Track; } }

namespace musik { namespace core { namespace plugin {

    /* adapt a track's tag store for call-scoped plugin metadata writes. */
    mc_metadata_writer CreateMetadataWriter(Track& store);

} } }
