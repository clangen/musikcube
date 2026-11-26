#pragma once

#include <musikcore/library.h>
#include <musikcore/library/LibraryBackend.h>

namespace musik { namespace core { namespace library { class Library; } } }

namespace musik { namespace core { namespace ffi {
    struct LibraryStatus;

    /* create the shared state behind a handle, choosing local or remote execution.
    existing results retain this instance when the app selects another library. */
    std::shared_ptr<const musik::core::library::Library> CreateLibrary(const LibraryBackend::Shared& library);

    /* read the selected library's current connection and configuration status. */
    LibraryStatus CaptureLibraryStatus(const LibraryBackend::Shared& library);

    /* open a consumer handle for a native library; also used by transport fixtures. */
#if defined(__GNUC__)
    __attribute__((visibility("default")))
#endif
    mc_result OpenLibraryHandle(const LibraryBackend::Shared& library, mc_library** out);
} } }
