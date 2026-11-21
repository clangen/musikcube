#ifndef MUSIKCORE_PATHS_HPP
#define MUSIKCORE_PATHS_HPP

#include <musikcore/paths.h>
#include <string>

namespace musik { namespace core { namespace api {

    inline std::string ReadPath(mc_path_kind kind) {
        std::string path;
        mc_path_read(kind, [](void* data, mc_string_view view) {
            static_cast<std::string*>(data)->assign(view.data, view.length);
        }, &path);
        return path;
    }

} } }

#endif
