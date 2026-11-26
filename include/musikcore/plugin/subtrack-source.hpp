#pragma once

#include <musikcore/plugin/media.h>
#include <musikcore/plugin.h>

#include <charconv>
#include <mutex>
#include <new>
#include <string>
#include <string_view>

namespace musik { namespace core { namespace plugin {

    /* exposes a track inside a multi-track file as its own playable URI. the
    decoder sees the selected track number and reads the original file through
    the host. Format supplies Prefix() for its URI scheme and SupportsPath()
    for the container formats it understands. each stream owns one child. */
    template<typename Format> class SubtrackSource {
    public:
        /* split scheme://number/container into a zero-based track and the
        path or URI the host should open. leave both outputs alone on failure. */
        static bool Parse(std::string_view uri, std::string& path, int& track) {
            const auto separator = uri.find("://");
            if (separator == std::string_view::npos || !separator || uri.substr(0, separator) != Format::Prefix()) {
                return false;
            }
            uri.remove_prefix(separator + 3);

            const auto slash = uri.find('/');
            if (slash == std::string_view::npos || slash == 0 || slash + 1 == uri.size()) {
                return false;
            }
            int number = 0;
            const auto end = uri.data() + slash;
            const auto result = std::from_chars(uri.data(), end, number);
            if (result.ec != std::errc() || result.ptr != end || uri.front() == '-') {
                return false;
            }
            const auto container = uri.substr(slash + 1);
            if (IsNestedSelector(container)) {
                return false;
            }
            path.assign(container);
            track = number;
            return true;
        }

        static mc_result Register(const mc_host_api& host) {
            static const mc_stream_vtable methods {
                sizeof(mc_stream_vtable),
                [](void* p) {
                    delete static_cast<SubtrackSource*>(p);
                },
                [](void* p, mc_string_view uri, mc_stream_open_flags flags, mc_error** error) -> mc_result {
                    try {
                        return static_cast<SubtrackSource*>(p)->Open(uri, flags, error);
                    }
                    catch (...) {
                        return MC_ERROR_INTERNAL;
                    }
                },
                [](void* p) -> mc_result {
                    auto& in = static_cast<SubtrackSource*>(p)->input;
                    return in.instance ? in.api->close(in.instance) : MC_OK;
                },
                [](void* p) {
                    static_cast<SubtrackSource*>(p)->Interrupt();
                },
                [](void* p, mc_stream_info* info) {
                    return static_cast<SubtrackSource*>(p)->Info(info);
                },
                [](void* p, void* dst, size_t size, size_t* read, mc_read_state* state, mc_error** error) {
                    auto& in = static_cast<SubtrackSource*>(p)->input;
                    return in.api->read(in.instance, dst, size, read, state, error);
                },
                [](void*, const void*, size_t, size_t* written, mc_error**) -> mc_result {
                    *written = 0;
                    return MC_ERROR_UNSUPPORTED;
                },
                [](void* p, uint64_t pos, mc_error** error) {
                    auto& in = static_cast<SubtrackSource*>(p)->input;
                    return in.api->seek(in.instance, pos, error);
                },
                [](void* p, uint64_t* pos) {
                    auto& in = static_cast<SubtrackSource*>(p)->input;
                    return in.api->tell(in.instance, pos);
                },
            };
            static const mc_stream_factory factory {
                sizeof(mc_stream_factory),
                nullptr,
                &methods,
                [](void*, const mc_media_hint* hint, uint32_t* supported) -> mc_result {
                    try {
                        std::string path;
                        int track;
                        *supported = hint->uri.data && Parse({ hint->uri.data, hint->uri.length }, path, track) &&
                            Format::SupportsPath(path);
                        return MC_OK;
                    }
                    catch (...) {
                        return MC_ERROR_INTERNAL;
                    }
                },
                [](void*, const mc_plugin_services* services, void** instance, mc_error**) -> mc_result {
                    if (!services->media) {
                        return MC_ERROR_UNSUPPORTED;
                    }
                    *instance = new (std::nothrow) SubtrackSource(*services->media);
                    return *instance ? MC_OK : MC_ERROR_INTERNAL;
                },
            };
            mc_plugin_capability capability = MC_PLUGIN_CAPABILITY_DEFAULT;
            capability.id = { MC_PLUGIN_CAPABILITY_STREAM, sizeof(MC_PLUGIN_CAPABILITY_STREAM) - 1 };
            capability.api = &factory;
            capability.api_size = sizeof(factory);
            return host.register_plugin_capability(host.host_context, &capability);
        }

    private:
        /* track numbers identify a selection inside the container. nesting
        another selector would feed a track URI back into stream discovery.
        HTTP URLs can have a numeric host, which is still a byte source. */
        static bool IsNestedSelector(std::string_view container) {
            const auto separator = container.find("://");
            if (separator == std::string_view::npos || separator == 0 ||
                container.find_first_of("/\\") < separator)
            {
                return false;
            }
            const auto scheme = container.substr(0, separator);
            if (scheme == "http" || scheme == "https") {
                return false;
            }
            container.remove_prefix(separator + 3);
            const auto slash = container.find('/');
            return slash != 0 && slash != std::string_view::npos &&
                container.substr(0, slash).find_first_not_of("0123456789") == std::string_view::npos;
        }

        explicit SubtrackSource(const mc_plugin_media_service& media)
        : media(media) {}
        ~SubtrackSource() {
            if (input.instance) {
                input.api->destroy(input.instance);
            }
        }

        mc_result Open(mc_string_view uri, mc_stream_open_flags flags, mc_error** error) {
            std::string path;
            int track;
            this->uri.assign(uri.data, uri.length);
            if (flags != MC_STREAM_OPEN_READ || !Parse(this->uri, path, track)) {
                return MC_ERROR_UNSUPPORTED;
            }
            const mc_string_view filename { path.data(), path.size() };
            mc_stream child = MC_STREAM_DEFAULT;
            const auto status = media.create_stream(media.context, filename, &child, error);
            if (status != MC_OK) {
                return status;
            }

            /* publish the unopened child before its first blocking call.
            if stop already arrived, destroy will release the unopened child. */
            {
                std::lock_guard lock(mutex);
                input = child;
                if (interrupted) {
                    return MC_ERROR_CANCELLED;
                }
            }
            return input.api->open(input.instance, filename, flags, error);
        }

        void Interrupt() {
            mc_stream pending;
            {
                std::lock_guard lock(mutex);
                interrupted = true;
                pending = input;
            }
            if (pending.instance) {
                pending.api->interrupt(pending.instance);
            }
        }

        mc_result Info(mc_stream_info* info) const {
            const auto status = input.api->get_info(input.instance, info);
            if (status == MC_OK) {
                info->uri = { uri.data(), uri.size() };
                info->flags &= ~MC_STREAM_WRITABLE;
            }
            return status;
        }

        const mc_plugin_media_service& media;
        mc_stream input = MC_STREAM_DEFAULT;
        std::string uri;
        std::mutex mutex;
        bool interrupted = false;
    };

    /* register the stream that exposes a decoder's individual subtracks. */
    template<typename Format> mc_result RegisterSubtrackSource(const mc_host_api& host) {
        return SubtrackSource<Format>::Register(host);
    }

} } }
