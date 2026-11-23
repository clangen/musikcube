#ifndef MUSIKCORE_PLUGIN_OUTPUT_HPP
#define MUSIKCORE_PLUGIN_OUTPUT_HPP

#include <musikcore/plugin.h>
#include <musikcore/plugin/output.h>

#include <type_traits>
#include <utility>

namespace musik { namespace core { namespace plugin {
    namespace detail {
        /* timing is optional; an implementation without it reports no device
        clock instead of inventing a playback position. */
        template<typename T, typename = void> struct HasOutputTiming : std::false_type {};

        template<typename T>
        struct HasOutputTiming<T,
            std::void_t<decltype(std::declval<T&>().GetTiming(std::declval<mc_output_timing&>()))>> : std::true_type {};

        /* generates static C trampolines for one C++ implementation type.
        no adapter object is created, copied, or moved; only T owns device state.
        tables live in the plugin image and import no core implementation symbols. */
        template<typename T> class OutputAdapter {
        public:
            static mc_result Register(const mc_host_api& host) noexcept {
                static_assert(std::is_nothrow_destructible_v<T>, "output destruction must not throw");
                static_assert(noexcept(std::declval<T&>().Stop()), "output Stop must be noexcept");
                static_assert(noexcept(std::declval<T&>().QueueAudioBuffer(std::declval<const mc_output_buffer&>())),
                    "output QueueAudioBuffer must be noexcept to preserve accepted ownership");
                static_assert(std::is_same_v<decltype(std::declval<T&>().QueueAudioBuffer(
                                                 std::declval<const mc_output_buffer&>())),
                                  mc_result>,
                    "output QueueAudioBuffer must return mc_result");
                static_assert(std::is_same_v<decltype(std::declval<T&>().Stop()), mc_result>,
                    "output Stop must return mc_result");
                static_assert(std::is_same_v<decltype(std::declval<const T&>().GetVolume()), double>,
                    "output GetVolume must return double");
                static constexpr mc_output_vtable methods { sizeof(mc_output_vtable), Destroy, Prepare,
                    QueueAudioBuffer, Pause, Resume, SetVolume, GetVolume, Stop, Drain, GetTiming };
                static constexpr mc_output_factory factory { sizeof(mc_output_factory), nullptr, &methods, VisitDevices,
                    ReadDefaultDevice, GetDefaultFormat, Create };
                mc_plugin_capability capability = MC_PLUGIN_CAPABILITY_DEFAULT;
                capability.id = { MC_PLUGIN_CAPABILITY_OUTPUT, sizeof(MC_PLUGIN_CAPABILITY_OUTPUT) - 1 };
                capability.api = &factory;
                capability.api_size = sizeof(factory);
                return host.register_capability(host.host_context, &capability);
            }

        private:
            OutputAdapter() = delete;

            /* contain ordinary implementation exceptions at the C boundary.
            queue, cleanup, and timing use their explicit noexcept contracts. */
            template<typename Work> static mc_result Invoke(Work&& work) noexcept {
                static_assert(std::is_same_v<decltype(work()), mc_result>, "output operations must return mc_result");
                try {
                    return work();
                }
                catch (...) {
                    return MC_ERROR_INTERNAL;
                }
            }

            static void MC_CALL Destroy(void* instance) noexcept { delete static_cast<T*>(instance); }

            static mc_result MC_CALL Prepare(void* instance, const mc_output_options* options,
                const mc_output_callbacks* callbacks, mc_error** error) noexcept
            {
                if (error) {
                    *error = nullptr;
                }
                return Invoke([&] {
                    return static_cast<T*>(instance)->Prepare(*options, *callbacks);
                });
            }

            static mc_result MC_CALL QueueAudioBuffer(void* instance, const mc_output_buffer* buffer) noexcept {
                /* admission may already have returned a token; never translate
                an exception here into a misleading unaccepted-buffer error. */
                return static_cast<T*>(instance)->QueueAudioBuffer(*buffer);
            }

            static mc_result MC_CALL Pause(void* instance) noexcept {
                return Invoke([&] {
                    return static_cast<T*>(instance)->Pause();
                });
            }

            static mc_result MC_CALL Resume(void* instance) noexcept {
                return Invoke([&] {
                    return static_cast<T*>(instance)->Resume();
                });
            }

            static mc_result MC_CALL SetVolume(void* instance, double volume) noexcept {
                return Invoke([&] {
                    return static_cast<T*>(instance)->SetVolume(volume);
                });
            }

            static mc_result MC_CALL GetVolume(void* instance, double* volume) noexcept {
                return Invoke([&]() -> mc_result {
                    *volume = static_cast<const T*>(instance)->GetVolume();
                    return MC_OK;
                });
            }

            static mc_result MC_CALL Stop(void* instance) noexcept { return static_cast<T*>(instance)->Stop(); }

            static mc_result MC_CALL Drain(void* instance, mc_error** error) noexcept
            {
                if (error) {
                    *error = nullptr;
                }
                return Invoke([&] {
                    return static_cast<T*>(instance)->Drain();
                });
            }

            static mc_result MC_CALL GetTiming(void* instance, mc_output_timing* timing) noexcept {
                *timing = MC_OUTPUT_TIMING_DEFAULT;
                if constexpr (HasOutputTiming<T>::value) {
                    static_assert(HasOutputTiming<const T>::value, "output GetTiming must be const");
                    static_assert(
                        std::is_same_v<decltype(std::declval<const T&>().GetTiming(std::declval<mc_output_timing&>())),
                            mc_result>,
                        "output GetTiming must return mc_result");
                    static_assert(noexcept(std::declval<const T&>().GetTiming(std::declval<mc_output_timing&>())),
                        "output GetTiming must be noexcept, bounded, and allocation-free");
                    return static_cast<const T*>(instance)->GetTiming(*timing);
                }
                return MC_OK;
            }

            static mc_result MC_CALL VisitDevices(void*, mc_output_device_fn read, void* user) noexcept {
                return Invoke([&] {
                    return T::VisitDevices(read, user);
                });
            }

            static mc_result MC_CALL ReadDefaultDevice(void*, mc_output_device_fn read, void* user) noexcept {
                return Invoke([&] {
                    return T::ReadDefaultDevice(read, user);
                });
            }

            static mc_result MC_CALL GetDefaultFormat(void*, mc_string_view device, mc_audio_format* format) noexcept {
                *format = MC_AUDIO_FORMAT_DEFAULT;
                return Invoke([&] {
                    return T::GetDefaultFormat(device, *format);
                });
            }

            static mc_result MC_CALL Create(
                void*, const mc_plugin_services* services, void** instance, mc_error** error) noexcept
            {
                if (error) {
                    *error = nullptr;
                }
                *instance = nullptr;
                return Invoke([&]() -> mc_result {
                    if constexpr (std::is_constructible_v<T, const mc_plugin_services*>) {
                        *instance = new T(services);
                    }
                    else {
                        static_assert(std::is_default_constructible_v<T>,
                            "output needs a default constructor or a const mc_plugin_services* constructor");
                        *instance = new T();
                    }
                    return MC_OK;
                });
            }
        };
    }

    /* register a stateless C factory implemented by T, using only public SDK
    headers. tables are immutable; each created T owns its device state. no base
    class, adapter object, compiled glue, thread, or core linkage is required.

    the host supplies validated arguments and negotiated struct prefixes,
    serializes control calls, and stops instances before reprepare/destruction.
    failed Prepare requires host-owned Stop/cleanup before reuse. the adapter
    forwards calls; it does not validate values or insert lifecycle operations.

    T returns mc_result from Prepare(const mc_output_options&, const mc_output_callbacks&),
    QueueAudioBuffer(const mc_output_buffer&) noexcept, Pause(), Resume(),
    SetVolume(double), Stop() noexcept, and Drain(). GetVolume() const returns double.
    Stop interrupts concurrent queue/drain and finishes callbacks. ordinary
    implementation exceptions become MC_ERROR_INTERNAL; optional errors stay null.
    noexcept queue/cleanup and a nonthrowing destructor preserve ABI/ownership rules.

    static device methods: VisitDevices(mc_output_device_fn, void*),
    ReadDefaultDevice(mc_output_device_fn, void*), and
    GetDefaultFormat(mc_string_view, mc_audio_format&), each returning mc_result.
    T has a default or const mc_plugin_services* constructor; services stay
    host-owned through destruction. constructors create unopened state.
    optional GetTiming(mc_output_timing&) const noexcept is nonblocking and
    allocation-free; absent timing reports unknown.

    usage during plugin initialization:
        return musik::core::plugin::RegisterOutput<MyOutput>(*host); */
    template<typename T> mc_result RegisterOutput(const mc_host_api& host) noexcept {
        return detail::OutputAdapter<T>::Register(host);
    }

} } }
#endif
