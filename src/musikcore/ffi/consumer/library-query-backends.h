#pragma once

#include <musikcore/library.h>
#include <musikcore/library/ILibrary.h>

struct LibraryInstance;

namespace musik { namespace core { namespace ffi {
    struct LibraryStatus;

    /* create the shared state behind a handle, choosing local or remote execution.
    existing results retain this instance when the app selects another library. */
    std::shared_ptr<const LibraryInstance> CreateLibraryInstance(const ILibraryPtr& library);

    /* read the selected library's current connection and configuration status. */
    LibraryStatus CaptureLibraryStatus(const ILibraryPtr& library);

    /* open a consumer handle for a native library; also used by transport fixtures. */
#if defined(__GNUC__)
    __attribute__((visibility("default")))
#endif
    mc_result OpenLibraryHandle(const ILibraryPtr& library, mc_library** out);
} } }
