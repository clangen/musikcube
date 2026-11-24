#pragma once

#include <musikcore/plugin/metadata.h>
#include <musikcore/sdk/ITagStore.h>

namespace musik { namespace core { namespace plugin {

    /* adapt a track's tag store for call-scoped plugin metadata writes. */
    mc_metadata_writer CreateMetadataWriter(sdk::ITagStore& store);

} } }
