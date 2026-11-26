#pragma once
#include <memory>
namespace musik { namespace core { namespace library { class Library; } } }
namespace musik { namespace core { namespace library {
    class LocalLibraryBackend;
    class RemoteLibraryBackend;
} } }
namespace musik { namespace core { namespace ffi { namespace detail {
    /* bind a local catalog and its worker to a new, independently scoped instance. */
    std::shared_ptr<const musik::core::library::Library> CreateLocalLibrary(const std::shared_ptr<library::LocalLibraryBackend>& library);

    /* bind the current connection; return empty when no connection is available. */
    std::shared_ptr<const musik::core::library::Library> CreateRemoteLibrary(const std::shared_ptr<library::RemoteLibraryBackend>& library);
} } } }
