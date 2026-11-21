#ifndef MUSIKCORE_VISUALIZER_HPP
#define MUSIKCORE_VISUALIZER_HPP

#include <musikcore/context.hpp>
#include <musikcore/visualizer.h>

#include <vector>

namespace musik { namespace core { namespace api {
    inline std::vector<std::string> ListVisualizers() {
        std::vector<std::string> result;
        const auto status = mc_visualizer_list([](void* data, const mc_string_view* names, size_t count) {
            auto& list = *static_cast<std::vector<std::string>*>(data);
            list.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                list.emplace_back(AsStringView(names[i]));
            }
        }, &result);
        if (status != MC_OK) {
            throw Error(status);
        }
        return result;
    }

    inline mc_result ShowVisualizer(size_t index) {
        return mc_visualizer_show(index);
    }

    inline mc_result HideVisualizer() {
        return mc_visualizer_hide();
    }
} } }

#endif
