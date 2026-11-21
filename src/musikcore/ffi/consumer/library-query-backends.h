#pragma once

#include <musikcore/library.h>
#include <musikcore/library/ILibrary.h>

struct LibraryIdentity;

namespace musik { namespace core { namespace ffi {
    struct LibraryStatus;

    /* Retain a concrete catalog and connection generation for typed queries. */
    std::shared_ptr<const LibraryIdentity> CaptureLibrary(const ILibraryPtr& library);
    LibraryStatus CaptureLibraryStatus(const ILibraryPtr& library);
    /* Native core entry point also used by injected-transport tests. */
#if defined(__GNUC__)
    __attribute__((visibility("default")))
#endif
    mc_result OpenLibraryHandle(const ILibraryPtr& library, mc_library** out);
} } }
