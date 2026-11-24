#pragma once

#include <common/DeleteDefaults.h>

#include <musikcore/plugin.h>
#include <musikcore/plugin/decoder.h>
#include <musikcore/plugin/dsp.h>
#include <musikcore/plugin/analyzer.h>
#include <musikcore/plugin/encoder.h>
#include <musikcore/plugin/output.h>
#include <musikcore/plugin/settings.h>
#include "PluginCallState.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace musik { namespace core { namespace ffi {

    class Plugin;

    /* owns C plugins for one engine session and records their features.
    construct, Initialize, and Shutdown on the same thread. initialization scans
    one directory once; create a new registry for another session.
    before Shutdown, stop/join all plugin users and destroy their instances;
    shutdown synchronously calls plugin cleanup and closes DLL handles.
    capability views do not extend this session's lifetime. feature calls may
    use workers during the session. reentering this session's lifecycle from
    its calls/callbacks is fatal; independent sessions remain usable.
    the registry cannot be copied or moved. */
    class PluginRegistry {
    public:
        using Logger = std::function<void(mc_log_level, mc_string_view, mc_string_view)>;

        /* copied metadata survives native unload, including disabled plugins
        that have been identified but never initialized. */
        struct PluginInfo {
            std::string filename;
            std::string id;
            std::string name;
            std::string version;
            std::string author;
            bool enabled;
        };

        using Selection = std::function<bool(const PluginInfo&)>;

        /* a typed non-owning factory view. copies and moves copy the view;
        finish calls and destroy created instances before registry Shutdown.
        supported C table types select their own capability ID, so callers
        cannot accidentally pair a decoder type with an output registration.
        Invoke makes unsafe same-session lifecycle reentry fatal; host callback
        exceptions propagate to the caller.
        it does not schedule or serialize work. */
        template<typename T> class Factory {
        public:
            Factory() = default;

            /* look up this table type in a running registry. absent plugins or
            unsupported features produce an empty view, with no allocation. */
            Factory(const PluginRegistry& registry, std::string_view pluginId)
            : api(static_cast<const T*>(registry.FindTable(pluginId, CapabilityId<T>())))
            , calls(&registry.calls) {}

            Factory(const Factory&) = default;
            Factory(Factory&&) noexcept = default;
            ~Factory() = default;

            Factory& operator=(const Factory&) = default;
            Factory& operator=(Factory&&) noexcept = default;

            /* checks for a populated view, not whether its session is alive. */
            explicit operator bool() const noexcept { return this->api != nullptr; }

            /* worker calls only; preserve the typed view if operation resets
            its original Factory. the engine session keeps the DLL loaded. */
            mc_result Invoke(const std::function<mc_result(const T&)>& operation) const {
                if (!this->api || !operation) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                const auto* api = this->api;
                PluginCallState::Scope scope(*this->calls);
                return operation(*api);
            }

        private:
            const T* api = nullptr;
            PluginCallState* calls = nullptr;
        };

        MUSIK_DELETE_COPY_AND_MOVE(PluginRegistry);

        /* logging may run on provider workers; consume text during the call
        and synchronize mutable state owned by the supplied sink. */
        explicit PluginRegistry(Logger logger = {});

        ~PluginRegistry();

        /* scan once in filename order and initialize selected plugins. selection
        receives copied metadata; false catalogs a disabled plugin. missing
        directories succeed empty. candidate failures are logged and skipped;
        directory/session failures return an error and close the failed session.
        invalid arguments leave an uninitialized registry unchanged. */
        mc_result Initialize(const std::string& directory, const Selection& select = {});

        std::vector<PluginInfo> ListPlugins() const;

        /* T selects the registered C factory family.
        return an empty view if the session is inactive or the plugin lacks T.
        pluginId chooses the plugin; T alone selects its validated capability. */
        template<typename T> Factory<T> Find(std::string_view pluginId) const { return Factory<T>(*this, pluginId); }

        /* callers first stop workers/instances and finish callbacks. cleanup
        and native handle closure complete synchronously before this returns. */
        /* full engine teardown checks reentry before destroying its services. */
        void AssertCanShutdown() const;
        mc_result Shutdown();

    private:
        enum class Phase { Created, Initializing, Running, Stopping, Stopped };

        /* keep the supported type-to-ID mapping closed: adding a family also
        requires its registration validator before callers can retrieve it. */
        template<typename T> static constexpr const char* CapabilityId() {
            if constexpr (std::is_same_v<T, mc_configuration_api>) {
                return MC_PLUGIN_CAPABILITY_CONFIGURATION;
            }
            else if constexpr (std::is_same_v<T, mc_stream_factory>) {
                return MC_PLUGIN_CAPABILITY_STREAM;
            }
            else if constexpr (std::is_same_v<T, mc_encoder_factory>) {
                return MC_PLUGIN_CAPABILITY_ENCODER;
            }
            else if constexpr (std::is_same_v<T, mc_decoder_factory>) {
                return MC_PLUGIN_CAPABILITY_DECODER;
            }
            else if constexpr (std::is_same_v<T, mc_dsp_factory>) {
                return MC_PLUGIN_CAPABILITY_DSP;
            }
            else if constexpr (std::is_same_v<T, mc_analyzer_factory>) {
                return MC_PLUGIN_CAPABILITY_ANALYZER;
            }
            else {
                static_assert(std::is_same_v<T, mc_output_factory>, "unsupported plugin factory type");
                return MC_PLUGIN_CAPABILITY_OUTPUT;
            }
        }

        const void* FindTable(std::string_view pluginId, const char* capabilityId) const;
        mc_result ValidateCallingThread() const;
        mc_result Discover(const std::string& directory, const Selection& select);
        mc_result Load(const std::string& filename, mc_error** error, const Selection& select);
        void LogLoadFailure(const std::string& filename, mc_result status, const std::string& message) const;

        Logger logger;
        mutable PluginCallState calls;
        mutable std::mutex mutex;
        Phase phase = Phase::Created;
        std::vector<std::unique_ptr<Plugin>> plugins;
        std::vector<PluginInfo> descriptions;
    };

} } }
