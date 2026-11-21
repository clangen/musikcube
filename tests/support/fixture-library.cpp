#include "CoreRuntime.h"

#include <musikcore/ffi/consumer/library-query-backends.h>

/* Fault-injection fixtures need a handle for their native playback transport. */
std::shared_ptr<const musik::core::api::Library> OpenFixtureLibrary(musik::core::ILibrary::Type kind) {
    using namespace musik::core;
    mc_library* handle = nullptr;
    const auto status = ffi::OpenLibraryHandle(LibraryFactory::Instance().DefaultLibrary(kind), &handle);
    if (status == MC_ERROR_IO || status == MC_ERROR_NOT_FOUND) {
        return {};
    }
    if (status != MC_OK) {
        throw api::Error(status);
    }

    api::Library library(handle);
    return std::make_shared<const api::Library>(std::move(library));
}
