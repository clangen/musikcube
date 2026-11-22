#include "ProviderHost.h"
#include "../common/error.h"

#include <condition_variable>
#include <climits>
#include <cstring>
#include <exception>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace musik { namespace core { namespace ffi {
    namespace {
        thread_local unsigned providerDepth = 0;

        /* contain even a nonconforming C++ provider's exception while its image
        is still mapped; no exception object may survive the call boundary. */
        template<typename Work>
        mc_result callProvider(Work&& work) noexcept {
            struct Scope {
                Scope() { ++providerDepth; }
                ~Scope() { --providerDepth; }
            } scope;
            try {
                return work();
            }
            catch (...) {
                return MC_ERROR_PLUGIN;
            }
        }

        /* reject malformed text before copying or presenting plugin metadata.
        size limits bound allocations at this trusted-but-validated boundary. */
        bool validText(mc_string_view text, size_t maximum = MC_PLUGIN_METADATA_MAX_BYTES) {
            if (text.length > maximum || (text.length && !text.data)) {
                return false;
            }
            size_t i = 0;
            while (i < text.length) {
                const auto first = static_cast<unsigned char>(text.data[i++]);
                if (!first) {
                    return false;
                }
                if (first < 0x80) {
                    continue;
                }
                unsigned remaining = 0;
                if (first >= 0xc2 && first <= 0xdf) {
                    remaining = 1;
                }
                else if (first >= 0xe0 && first <= 0xef) {
                    remaining = 2;
                }
                else if (first >= 0xf0 && first <= 0xf4) {
                    remaining = 3;
                }
                if (!remaining || remaining > text.length - i) {
                    return false;
                }
                const unsigned minimum = remaining == 1 ? 0x80 : remaining == 2 ? 0x800
                                                                                : 0x10000;
                unsigned value = first & (0x7f >> (remaining + 1));
                while (remaining--) {
                    const auto byte = static_cast<unsigned char>(text.data[i++]);
                    if ((byte & 0xc0) != 0x80) {
                        return false;
                    }
                    value = (value << 6) | (byte & 0x3f);
                }
                if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
                    return false;
                }
            }
            return true;
        }

        bool validId(mc_string_view id) {
            if (!id.length || !validText(id, MC_PLUGIN_ID_MAX_BYTES)) {
                return false;
            }
            for (size_t i = 0; i < id.length; ++i) {
                const char c = id.data[i];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
                {
                    return false;
                }
            }
            return true;
        }

        bool compatible(uint32_t version) {
            return version != 0 && MC_ABI_VERSION_MAJOR(version) == MC_ABI_VERSION_MAJOR(MC_PLUGIN_ABI_VERSION) &&
                MC_ABI_VERSION_MINOR(version) <= MC_ABI_VERSION_MINOR(MC_PLUGIN_ABI_VERSION);
        }

        /* native handles never escape into the public C tables. */
        class NativeLibrary {
            public:
                explicit NativeLibrary(const std::string& filename) {
#ifdef _WIN32
                    if (filename.size() >= static_cast<size_t>(INT_MAX)) {
                        return;
                    }
                    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                        filename.c_str(), -1, nullptr, 0);
                    if (!length) {
                        return;
                    }
                    std::vector<wchar_t> wide(static_cast<size_t>(length));
                    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename.c_str(), -1, wide.data(), length);
                    this->handle = LoadLibraryExW(wide.data(), nullptr,
                        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
                    this->handle = dlopen(filename.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
                }
                ~NativeLibrary() {
                    if (this->handle) {
#ifdef _WIN32
                        FreeLibrary(this->handle);
#else
                        dlclose(this->handle);
#endif
                    }
                }
                NativeLibrary(const NativeLibrary&) = delete;
                NativeLibrary& operator=(const NativeLibrary&) = delete;

                bool IsOpen() const { return this->handle != nullptr; }
                bool IsSame(const NativeLibrary& other) const { return this->handle == other.handle; }
                mc_plugin_get_api_fn Entry() const {
#ifdef _WIN32
                    return reinterpret_cast<mc_plugin_get_api_fn>(GetProcAddress(this->handle, "mc_plugin_get_api"));
#else
                    return reinterpret_cast<mc_plugin_get_api_fn>(dlsym(this->handle, "mc_plugin_get_api"));
#endif
                }

            private:
#ifdef _WIN32
                HMODULE handle = nullptr;
#else
                void* handle = nullptr;
#endif
        };

        /* the optional diagnostic is core-owned and may outlive native unload. */
        mc_result transferError(mc_result status, mc_error* detail, mc_error** error) {
            if (detail && mc_error_get_code(detail) != status) {
                mc_error_destroy(&detail);
                status = MC_ERROR_PLUGIN;
            }
            if (error && detail) {
                *error = detail;
            }
            else {
                mc_error_destroy(&detail);
                fail(status, "Provider initialization or negotiation failed", error);
            }
            return status;
        }
    }

    struct ProviderHost::Module {
        enum class Phase {
            Negotiating,
            Initializing,
            Running,
            Stopping,
            Stopped
        };

        struct Registration {
            std::string id;
            const void* api;
            uint32_t size;
            uint32_t version;
        };

        explicit Module(const std::string& filename, Logger logger)
        : native(filename)
        , logger(std::move(logger))
        , owner(std::this_thread::get_id()) {
            this->api.struct_size = static_cast<uint32_t>(sizeof(this->api));
            this->host.host_context = this;
            this->host.log = Log;
            this->host.create_error = CreateError;
            this->host.destroy_error = DestroyError;
            this->host.register_capability = Register;
        }
        ~Module() { this->Stop(); }

        /* callbacks never run while the module mutex is held. initialization
        rollback uses this same path, so partial registrations never escape. */
        mc_result Stop() {
            {
                std::unique_lock<std::mutex> lock(this->mutex);
                if (this->phase == Phase::Stopped) {
                    return MC_OK;
                }
                this->phase = Phase::Stopping;
                this->drained.wait(lock, [&] {
                    return !this->references && !this->calls;
                });
                this->registrations.clear();
            }
            mc_result status = MC_OK;
            if (this->needsShutdown) {
                this->needsShutdown = false;
                status = callProvider([&] {
                    this->api.shutdown(this->pluginContext);
                    return MC_OK;
                });
            }
            this->phase = Phase::Stopped;
            return status;
        }

        static void MC_CALL Log(void* context, mc_log_level level, mc_string_view tag, mc_string_view text) noexcept {
            auto& module = *static_cast<Module*>(context);
            if (module.logger && level <= MC_LOG_ERROR && validText(tag) && validText(text, MC_PLUGIN_DIAGNOSTIC_MAX_BYTES)) {
                try {
                    module.logger(level, tag, text);
                }
                catch (...) {
                    /* diagnostics cannot unwind through provider code. */
                }
            }
        }

        static mc_result MC_CALL CreateError(void*, mc_result code, mc_string_view message, mc_error** error) noexcept {
            if (!error) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            *error = nullptr;
            if (code == MC_OK || !validText(message, MC_PLUGIN_DIAGNOSTIC_MAX_BYTES)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            try {
                const std::string text(message.data ? message.data : "", message.length);
                fail(code, text.c_str(), error);
                return *error ? MC_OK : MC_ERROR_INTERNAL;
            }
            catch (...) {
                return MC_ERROR_INTERNAL;
            }
        }

        static void MC_CALL DestroyError(void*, mc_error** error) noexcept {
            mc_error_destroy(error);
        }

        static mc_result MC_CALL Register(void* context, const mc_plugin_capability* capability) noexcept {
            auto& module = *static_cast<Module*>(context);
            if (std::this_thread::get_id() != module.owner) {
                return MC_ERROR_WRONG_THREAD;
            }
            return exception_barrier(nullptr, [&]() -> mc_result {
                std::lock_guard<std::mutex> lock(module.mutex);
                if (module.phase != Phase::Initializing) {
                    return MC_ERROR_IN_FLIGHT;
                }
                if (!capability || capability->struct_size < sizeof(mc_plugin_capability) ||
                    !validId(capability->id) || !capability->api ||
                    capability->api_size < sizeof(uint32_t))
                {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                uint32_t declaredSize = 0;
                std::memcpy(&declaredSize, capability->api, sizeof(declaredSize));
                if (declaredSize != capability->api_size) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                const std::string id(capability->id.data, capability->id.length);
                for (const auto& registration : module.registrations) {
                    if (registration.id == id) {
                        return MC_ERROR_INVALID_ARGUMENT;
                    }
                }
                module.registrations.push_back({ id, capability->api, capability->api_size, module.api.abi_version });
                return MC_OK;
            });
        }

        NativeLibrary native;
        Logger logger;
        const std::thread::id owner;
        std::string id;
        mc_host_api host = MC_HOST_API_DEFAULT;
        mc_plugin_api api {};
        void* pluginContext = nullptr;
        std::mutex mutex;
        std::condition_variable drained;
        Phase phase = Phase::Negotiating;
        bool needsShutdown = false;
        size_t references = 0;
        size_t calls = 0;
        std::vector<Registration> registrations;
    };

    /* one logical acquisition, shared by Capability copies and active calls.
    its final release wakes owner-thread teardown without invoking plugin code. */
    struct ProviderHost::Lease {
        Lease(std::shared_ptr<Module> module, Module::Registration registration)
        : module(std::move(module))
        , registration(std::move(registration)) {}
        ~Lease() {
            std::lock_guard<std::mutex> lock(this->module->mutex);
            --this->module->references;
            this->module->drained.notify_all();
        }
        std::shared_ptr<Module> module;
        Module::Registration registration;
    };

    ProviderHost::ProviderHost(Logger logger)
    : logger(std::move(logger))
    , owner(std::this_thread::get_id()) {
    }

    ProviderHost::~ProviderHost() {
        /* returning from a wrong-thread/reentrant destructor would either
        strand providers or unload executing code. explicit Shutdown reports it. */
        const auto status = this->Shutdown();
        if (status == MC_ERROR_WRONG_THREAD || status == MC_ERROR_IN_FLIGHT) {
            std::terminate();
        }
    }

    mc_result ProviderHost::Load(const std::string& filename, mc_error** error) {
        return exception_barrier(error, [&]() -> mc_result {
            if (std::this_thread::get_id() != this->owner) {
                return fail(MC_ERROR_WRONG_THREAD, "Provider loading requires its owner thread", error);
            }
            if (providerDepth) {
                return fail(MC_ERROR_IN_FLIGHT, "Provider lifecycle cannot reenter a provider call", error);
            }
            {
                std::lock_guard<std::mutex> lock(this->mutex);
                if (this->stopping) {
                    return MC_ERROR_CANCELLED;
                }
            }
            if (filename.empty() || filename.find('\0') != std::string::npos) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            auto module = std::make_shared<Module>(filename, this->logger);
            if (!module->native.IsOpen()) {
                return fail(MC_ERROR_IO, "Could not load provider library", error);
            }
            /* reentering the same native image could overwrite its static host
            pointer before a duplicate ID is detected. reject aliases by handle. */
            {
                std::lock_guard<std::mutex> lock(this->mutex);
                for (const auto& existing : this->modules) {
                    if (existing->native.IsSame(module->native)) {
                        return fail(MC_ERROR_PLUGIN, "Provider image already loaded", error);
                    }
                }
            }
            const auto entry = module->native.Entry();
            if (!entry) {
                return MC_ERROR_UNSUPPORTED;
            }
            mc_error* detail = nullptr;
            auto status = callProvider([&] {
                return entry(MC_PLUGIN_ABI_VERSION, &module->host, &module->api, &detail);
            });
            if (status != MC_OK) {
                return transferError(status, detail, error);
            }
            if (detail) {
                mc_error_destroy(&detail);
                return fail(MC_ERROR_PLUGIN, "Provider returned an error with success", error);
            }
            /* inspect the declared prefix before touching any required tail field. */
            if (module->api.struct_size != sizeof(mc_plugin_api) ||
                !compatible(module->api.abi_version) || !validId(module->api.id) ||
                !module->api.name.length || !validText(module->api.name) || !validText(module->api.version) ||
                !validText(module->api.author) || !module->api.initialize || !module->api.shutdown)
            {
                return fail(MC_ERROR_PLUGIN, "Invalid or incompatible provider table", error);
            }
            module->id.assign(module->api.id.data, module->api.id.length);
            {
                std::lock_guard<std::mutex> lock(this->mutex);
                for (const auto& existing : this->modules) {
                    if (existing->id == module->id) {
                        return fail(MC_ERROR_PLUGIN, "Duplicate provider ID", error);
                    }
                }
            }
            module->phase = Module::Phase::Initializing;
            module->needsShutdown = true;
            status = callProvider([&] {
                return module->api.initialize(&module->host, &module->pluginContext, &detail);
            });
            if (status != MC_OK) {
                return transferError(status, detail, error);
            }
            if (detail) {
                mc_error_destroy(&detail);
                return fail(MC_ERROR_PLUGIN, "Provider returned an error with success", error);
            }
            /* publication is the commit point; no partial factory set is visible. */
            module->phase = Module::Phase::Running;
            std::lock_guard<std::mutex> lock(this->mutex);
            this->modules.push_back(std::move(module));
            return MC_OK;
        });
    }

    ProviderHost::Capability ProviderHost::Acquire(const std::string& pluginId, const std::string& capabilityId) {
        std::lock_guard<std::mutex> lock(this->mutex);
        if (!this->stopping) {
            for (const auto& module : this->modules) {
                if (module->id != pluginId) {
                    continue;
                }
                std::lock_guard<std::mutex> moduleLock(module->mutex);
                for (const auto& registration : module->registrations) {
                    if (registration.id == capabilityId) {
                        auto lease = std::make_shared<Lease>(module, registration);
                        ++module->references;
                        return Capability(std::move(lease));
                    }
                }
            }
        }
        return Capability();
    }

    mc_result ProviderHost::Capability::Invoke(
        const std::function<mc_result(const void*, uint32_t, uint32_t)>& operation) const
    {
        /* keep a local pin even when operation resets the original Capability. */
        const auto pin = this->lease;
        if (!pin || !operation) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        const auto module = pin->module;
        {
            std::lock_guard<std::mutex> lock(module->mutex);
            if (module->phase != Module::Phase::Running) {
                return MC_ERROR_CANCELLED;
            }
            ++module->calls;
        }
        const auto status = callProvider([&] {
            const auto& registration = pin->registration;
            return operation(registration.api, registration.size, registration.version);
        });
        {
            std::lock_guard<std::mutex> lock(module->mutex);
            --module->calls;
            module->drained.notify_all();
        }
        return status;
    }

    mc_result ProviderHost::Shutdown() {
        if (std::this_thread::get_id() != this->owner) {
            return MC_ERROR_WRONG_THREAD;
        }
        if (providerDepth) {
            return MC_ERROR_IN_FLIGHT;
        }
        std::vector<std::shared_ptr<Module>> retired;
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            this->stopping = true;
            retired.swap(this->modules);
            /* close admission across the entire registry before draining one
            provider, so another provider cannot prolong teardown with new calls. */
            for (const auto& module : retired) {
                std::lock_guard<std::mutex> moduleLock(module->mutex);
                module->phase = Module::Phase::Stopping;
            }
        }
        mc_result status = MC_OK;
        for (const auto& module : retired) {
            const auto stopped = module->Stop();
            if (stopped != MC_OK) {
                status = stopped;
            }
        }
        return status;
    }
} } }
