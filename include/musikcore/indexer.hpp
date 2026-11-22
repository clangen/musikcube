#ifndef MUSIKCORE_INDEXER_HPP
#define MUSIKCORE_INDEXER_HPP

#include <musikcore/context.hpp>
#include <musikcore/indexer.h>
#include <vector>

namespace musik { namespace core { namespace api {
    /* Borrows a context. Copy and move construction preserve the local target.
       Assignment is disabled. */
    class Indexer {
    public:
        explicit Indexer(Context& context) : context(context) {}

        mc_indexer_state State() const {
            mc_indexer_state state = MC_INDEXER_STOPPED;
            mc_indexer_get_state(this->context.Get(), &state);
            return state;
        }

        std::vector<std::string> Paths() const {
            std::vector<std::string> paths;
            const auto status = mc_indexer_read_paths(this->context.Get(),
                [](void* data, const mc_string_view* values, size_t count) {
                    auto& paths = *static_cast<std::vector<std::string>*>(data);
                    paths.reserve(count);
                    for (size_t i = 0; i < count; ++i) {
                        paths.emplace_back(AsStringView(values[i]));
                    }
                }, &paths);
            if (status != MC_OK && status != MC_ERROR_UNSUPPORTED) {
                throw Error(status);
            }
            return paths;
        }

        mc_result AddPath(std::string_view path) {
            return mc_indexer_add_path(this->context.Get(), { path.data(), path.size() });
        }

        mc_result RemovePath(std::string_view path) {
            return mc_indexer_remove_path(this->context.Get(), { path.data(), path.size() });
        }

        mc_result Schedule(mc_indexer_scan scan) {
            return mc_indexer_schedule(this->context.Get(), scan);
        }

        mc_result Cancel() {
            return mc_indexer_cancel(this->context.Get());
        }

        mc_result SetCallback(mc_indexer_event_fn callback, void* data = nullptr) {
            return mc_indexer_set_callback(this->context.Get(), callback, data);
        }

    private:
        Context& context;
    };
} } }

#endif
