#pragma once

#include <common/DeleteDefaults.h>

#include "PluginRegistry.h"

#include <optional>
#include <string_view>

namespace musik { namespace core { namespace ffi {

    class NativeLibrary;

    /* one DLL and its state, owned by the registry for an engine session.
    Identify validates its entry table; Initialize registers its features.
    lifecycle calls stay on the opening thread; feature calls may use workers.
    stop all users and destroy their instances before Stop unloads the DLL.
    lifecycle reentry from this plugin's active call or callback is fatal.
    instances have unique ownership and cannot be copied or moved. */
    class Plugin {
    public:
        /* snapshot of a registered immutable table, copied for lookup. the table is
        non-owning and remains usable only during this plugin's live session. */
        struct CapabilityTable {
            const void* api;
            uint32_t size;
            uint32_t version;
        };

        MUSIK_DELETE_COPY_AND_MOVE(Plugin);

        ~Plugin();

        static std::unique_ptr<Plugin> Open(const std::string& filename, PluginRegistry::Logger logger = {});
        bool IsSameImage(const Plugin& other) const;
        bool HasId(std::string_view id) const;
        PluginRegistry::PluginInfo Describe(bool enabled) const;
        const mc_host_api& Host() const { return this->host; }

        mc_result Identify(mc_error** error = nullptr);
        mc_result Initialize(mc_error** error = nullptr);
        /* return only successfully initialized registrations; absent before
        initialization or after Stop. the snapshot does not extend DLL lifetime. */
        std::optional<CapabilityTable> FindCapability(std::string_view capabilityId) const;
        mc_result Stop();

    private:
        enum class Phase { Opened, Identified, Initializing, Running, Failed, Stopped };

        struct Registration {
            std::string id;
            const void* api;
            uint32_t size;
            uint32_t version;
        };

        explicit Plugin(const std::string& filename, PluginRegistry::Logger logger);

        mc_result ValidateCallingThread() const;

        static void MC_CALL Log(void*, mc_log_level, mc_string_view, mc_string_view) noexcept;
        static mc_result MC_CALL CreateError(void*, mc_result, mc_string_view, mc_error**) noexcept;
        static void MC_CALL DestroyError(void*, mc_error**) noexcept;
        static mc_result MC_CALL Register(void*, const mc_plugin_capability*) noexcept;

        const std::unique_ptr<NativeLibrary> native;
        const PluginRegistry::Logger logger;
        mutable PluginCallState calls;
        const mc_host_api host;
        mc_plugin_api api {};
        PluginRegistry::PluginInfo metadata;
        void* pluginContext = nullptr;
        mutable std::mutex mutex;
        Phase phase = Phase::Opened;
        bool needsShutdown = false;
        std::vector<Registration> registrations;
    };

} } }
