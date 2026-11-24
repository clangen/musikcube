#pragma once

#include <musikcore/plugin/encoder.h>

#include <string>

namespace musik { namespace core { namespace plugin {

    namespace detail {
        template<typename Work> mc_result encoderCall(Work&& work) noexcept {
            try {
                return work();
            }
            catch (...) {
                return MC_ERROR_INTERNAL;
            }
        }
    }

    /* adapt a C++ streaming encoder that returns encoded bytes from Encode
    and Flush. Release destroys the encoder and Finalize updates a saved file. */
    template<typename Encoder> const mc_encoder_vtable& StreamingEncoderApi() {
        static const mc_encoder_vtable methods {
            sizeof(mc_encoder_vtable),
            [](void* instance) { static_cast<Encoder*>(instance)->Release(); },
            1,
            [](void* instance, const mc_stream*, uint32_t rate, uint32_t channels, uint32_t bitrate) -> mc_result {
                return detail::encoderCall([&]() -> mc_result {
                    return static_cast<Encoder*>(instance)->Initialize(rate, channels, bitrate)
                        ? MC_OK
                        : MC_ERROR_PLUGIN;
                });
            },
            [](void* instance, const mc_pcm_view* pcm, mc_encoded_block* block) {
                return detail::encoderCall([&]() -> mc_result {
                    char* bytes = nullptr;
                    const auto count = static_cast<Encoder*>(instance)->Encode(*pcm, &bytes);
                    if (count < 0) {
                        return MC_ERROR_PLUGIN;
                    }
                    block->data = bytes;
                    block->length = static_cast<size_t>(count);
                    return MC_OK;
                });
            },
            [](void* instance, mc_encoded_block* block) {
                return detail::encoderCall([&]() -> mc_result {
                    char* bytes = nullptr;
                    const auto count = static_cast<Encoder*>(instance)->Flush(&bytes);
                    if (count < 0) {
                        return MC_ERROR_PLUGIN;
                    }
                    block->data = bytes;
                    block->length = static_cast<size_t>(count);
                    return MC_OK;
                });
            },
            [](void* instance, mc_string_view uri) {
                return detail::encoderCall([&]() -> mc_result {
                    std::string path(uri.data ? uri.data : "", uri.length);
                    return static_cast<Encoder*>(instance)->Finalize(path.c_str()) ? MC_OK : MC_ERROR_PLUGIN;
                });
            },
        };
        return methods;
    }

    /* adapt a C++ encoder that writes a complete file to the host stream.
    Release destroys the encoder and Finalize writes any delayed output. */
    template<typename Encoder> const mc_encoder_vtable& BlockingEncoderApi() {
        static const mc_encoder_vtable methods {
            sizeof(mc_encoder_vtable),
            [](void* instance) { static_cast<Encoder*>(instance)->Release(); },
            0,
            [](void* instance, const mc_stream* output, uint32_t rate, uint32_t channels, uint32_t bitrate) {
                return detail::encoderCall([&]() -> mc_result {
                    return static_cast<Encoder*>(instance)->Initialize(*output, rate, channels, bitrate)
                        ? MC_OK
                        : MC_ERROR_PLUGIN;
                });
            },
            [](void* instance, const mc_pcm_view* pcm, mc_encoded_block*) {
                return detail::encoderCall([&]() -> mc_result {
                    return static_cast<Encoder*>(instance)->Encode(*pcm) ? MC_OK : MC_ERROR_PLUGIN;
                });
            },
            [](void*, mc_encoded_block*) -> mc_result { return MC_OK; },
            [](void* instance, mc_string_view) {
                return detail::encoderCall([&]() -> mc_result {
                    return static_cast<Encoder*>(instance)->Finalize() ? MC_OK : MC_ERROR_PLUGIN;
                });
            },
        };
        return methods;
    }

} } }
