#pragma once

#include <musikcore/plugin.h>
#include <musikcore/plugin/decoder.h>

#include <atomic>
#include <type_traits>
#include <utility>

namespace musik { namespace core { namespace plugin {

    /* one decode call's destination. Write gives the codec interleaved floats
    in host storage and publishes exactly the frames the codec produced. the
    view and its copies expire when decode returns. */
    class DecoderBuffer {
    public:
        DecoderBuffer(const mc_audio_scratch_buffer_api& api, mc_audio_scratch_buffer& buffer)
        : api(api)
        , buffer(buffer) {}

        /* fill returns a status and sets the number of complete frames. it
        cannot throw while it has access to the host's sample storage. */
        template<typename Fill> mc_result Write(uint32_t rate, uint32_t channels, size_t capacity, Fill&& fill) {
            static_assert(std::is_nothrow_invocable_r_v<mc_result, Fill, float*, size_t&>);
            const mc_audio_format format { sizeof(mc_audio_format), rate, channels, 0 };
            auto status = api.set_format(api.context, &buffer, &format);
            if (status == MC_OK) {
                status = api.reserve(api.context, &buffer, capacity);
            }
            mc_audio_buffer_view view = MC_AUDIO_BUFFER_VIEW_DEFAULT;
            if (status == MC_OK) {
                status = api.begin_write(api.context, &buffer, &view);
            }
            if (status != MC_OK) {
                return status;
            }
            size_t count = 0;
            status = fill(view.data, count);
            const auto finished = api.end_write(api.context, &buffer, status == MC_OK ? count : 0);
            return status == MC_OK ? finished : status;
        }

    private:
        const mc_audio_scratch_buffer_api& api;
        mc_audio_scratch_buffer& buffer;
    };

    namespace detail {
        template<typename T, typename = void> struct HasDecoderFormat : std::false_type {};
        template<typename T>
        struct HasDecoderFormat<T, std::void_t<decltype(std::declval<T&>().GetFormat(
            std::declval<mc_audio_format&>()))>> : std::true_type {};
        /* owns one codec per stream. interrupt latches independently of the
        worker; the host wakes the input stream if the codec is reading. */
        template<typename Codec> struct DecoderInstance {
            explicit DecoderInstance(const mc_plugin_services& services)
            : codec(services)
            , scratch(*services.audio_scratch) {}

            Codec codec;
            const mc_audio_scratch_buffer_api& scratch;
            std::atomic<bool> interrupted { false };
        };

        template<typename Work> mc_result decoderCall(Work&& work) noexcept {
            try {
                return work();
            }
            catch (...) {
                return MC_ERROR_INTERNAL;
            }
        }

        template<typename Codec, typename Work> mc_result useDecoder(void* opaque, Work&& work) noexcept {
            auto& instance = *static_cast<DecoderInstance<Codec>*>(opaque);
            if (instance.interrupted.load()) {
                return MC_ERROR_CANCELLED;
            }
            const auto result = decoderCall([&] {
                return work(instance);
            });
            return instance.interrupted.load() ? MC_ERROR_CANCELLED : result;
        }
    }

    /* register a C++ codec with the host. Codec supplies CanHandle, Open,
    Decode, Seek, GetDuration, and SetPreferredSampleRate. calls on a codec
    are serialized; the wrapper owns each instance from create through destroy. */
    template<typename Codec> mc_result RegisterDecoder(const mc_host_api& host) {
        using Instance = detail::DecoderInstance<Codec>;
        static const mc_decoder_vtable methods {
            sizeof(mc_decoder_vtable),
            [](void* instance) {
                delete static_cast<Instance*>(instance);
            },
            [](void* instance, const mc_stream* stream, mc_error**) {
                return detail::useDecoder<Codec>(instance, [&](auto& self) {
                    return self.codec.Open(*stream);
                });
            },
            [](void* instance) {
                static_cast<Instance*>(instance)->interrupted.store(true);
            },
            [](void* instance, mc_audio_scratch_buffer* buffer, mc_decode_state* state, mc_error**) {
                return detail::useDecoder<Codec>(instance, [&](auto& self) {
                    DecoderBuffer target(self.scratch, *buffer);
                    return self.codec.Decode(target, *state);
                });
            },
            [](void* instance, double seconds, double* actual, mc_error**) {
                return detail::useDecoder<Codec>(instance, [&](auto& self) {
                    return self.codec.Seek(seconds, *actual);
                });
            },
            [](void* instance, double* seconds) {
                return detail::useDecoder<Codec>(instance, [&](auto& self) {
                    return self.codec.GetDuration(*seconds);
                });
            },
            [](void* instance, uint32_t rate) {
                return detail::useDecoder<Codec>(instance, [&](auto& self) {
                    return self.codec.SetPreferredSampleRate(rate);
                });
            },
            [](void* instance, mc_audio_format* format) -> mc_result {
                if constexpr (detail::HasDecoderFormat<Codec>::value) {
                    return detail::useDecoder<Codec>(instance, [&](auto& self) {
                        return self.codec.GetFormat(*format);
                    });
                }
                return MC_ERROR_UNSUPPORTED;
            },
        };
        static const mc_decoder_factory factory {
            sizeof(mc_decoder_factory),
            nullptr,
            &methods,
            [](void*, const mc_media_hint* hint, uint32_t* supported) {
                return detail::decoderCall([&] {
                    *supported = Codec::CanHandle(*hint);
                    return MC_OK;
                });
            },
            [](void*, const mc_plugin_services* services, void** instance, mc_error**) {
                return detail::decoderCall([&] {
                    *instance = new Instance(*services);
                    return MC_OK;
                });
            },
        };
        mc_plugin_capability capability = MC_PLUGIN_CAPABILITY_DEFAULT;
        capability.id = { MC_PLUGIN_CAPABILITY_DECODER, sizeof(MC_PLUGIN_CAPABILITY_DECODER) - 1 };
        capability.api = &factory;
        capability.api_size = sizeof(factory);
        return host.register_plugin_capability(host.host_context, &capability);
    }

} } }
