#pragma once
#include <memory>
struct LibraryInstance;
namespace musik { namespace core { namespace library {
    class LocalLibrary;
    class RemoteLibrary;
} } }
namespace musik { namespace core { namespace ffi { namespace detail {
    /* bind a local catalog and its worker to a new, independently scoped instance. */
    std::shared_ptr<const LibraryInstance> CreateLocalLibraryInstance(const std::shared_ptr<library::LocalLibrary>& library);

    /* bind the current connection; return empty when no connection is available. */
    std::shared_ptr<const LibraryInstance> CreateRemoteLibraryInstance(const std::shared_ptr<library::RemoteLibrary>& library);
} } } }
