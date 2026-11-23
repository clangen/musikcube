#pragma once

#include <musikcore/plugin/stream.h>

namespace musik { namespace core { namespace io {

    /* built-in fallback for filesystem paths. create the stream before open
    so playback can publish it to the thread that handles Stop. */
    mc_result CreateLocalFileSource(mc_stream& stream);

} } }
