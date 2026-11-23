#include "PluginRegistry.h"
#include "Plugin.h"
#include <common/Contract.h>
#include "../common/error.h"

#include <algorithm>
#include <exception>
#include <filesystem>

namespace musik { namespace core { namespace ffi {
    namespace {
        /* filesystem UTF-8 strings use char8_t in C++20, while the ABI uses
        char bytes. preserve their encoding without a locale-dependent conversion. */
        std::string pathText(const std::filesystem::path& path) {
            const auto bytes = path.u8string();
            return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
        }
    }

    PluginRegistry::PluginRegistry(Logger logger)
    : logger(std::move(logger)) {
    }

    PluginRegistry::~PluginRegistry() {
        const auto status = this->Shutdown();
        musik::contract::Assert(status == MC_OK, "PluginRegistry destruction must run on its owning thread");
    }

    mc_result PluginRegistry::ValidateCallingThread() const {
        if (!this->calls.IsOwnerThread()) {
            return MC_ERROR_WRONG_THREAD;
        }
        musik::contract::Assert(!this->calls.IsActive(), "PluginRegistry initialization reentered an active session");

        return MC_OK;
    }

    mc_result PluginRegistry::Initialize(const std::string& directory, const Selection& select) {
        const auto admission = this->ValidateCallingThread();
        if (admission != MC_OK) {
            return admission;
        }
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            if (this->phase == Phase::Stopped) {
                return MC_ERROR_CANCELLED;
            }
            if (this->phase != Phase::Created) {
                return MC_ERROR_IN_FLIGHT;
            }
            if (directory.empty() || directory.find('\0') != std::string::npos) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            this->phase = Phase::Initializing;
        }

        /* complete scanning before loading candidates. fatal failures close this
        session and roll back any candidates accepted before an allocation failure. */
        const auto result = exception_barrier(nullptr, [&] {
            PluginCallState::Scope scope(this->calls);
            return this->Discover(directory, select);
        });
        if (result != MC_OK) {
            this->Shutdown();
            std::lock_guard<std::mutex> lock(this->mutex);
            this->descriptions.clear();
            return result;
        }
        std::lock_guard<std::mutex> lock(this->mutex);
        this->phase = Phase::Running;
        return MC_OK;
    }

    void PluginRegistry::LogLoadFailure(
        const std::string& filename, mc_result status, const std::string& message) const
    {
        /* libraries without the plugin entry are ordinary discovery bystanders.
        diagnostics must not throw or reenter lifecycle work during initialization. */
        if (this->logger && status != MC_ERROR_UNSUPPORTED) {
            const auto text = filename + ": " + message + " (status " + std::to_string(status) + ")";
            static constexpr char tag[] = "PluginRegistry";
            PluginCallState::Scope scope(this->calls);
            try {
                this->logger(MC_LOG_ERROR, { tag, sizeof(tag) - 1 }, { text.data(), text.size() });
            }
            catch (...) {
                /* a failed diagnostic sink must not break cleanup or recursively
                report another failure through that same sink. */
            }
        }
    }

    mc_result PluginRegistry::Load(const std::string& filename, mc_error** error, const Selection& select) {
        return exception_barrier(error, [&]() -> mc_result {
            Logger sink;
            if (this->logger) {
                sink = [this](mc_log_level level, mc_string_view tag, mc_string_view text) {
                    PluginCallState::Scope scope(this->calls);
                    this->logger(level, tag, text);
                };
            }
            auto plugin = Plugin::Open(filename, std::move(sink));
            if (!plugin) {
                return fail(MC_ERROR_IO, "Could not load plugin library", error);
            }

            /* reject native aliases before entry can change image-global state.
            no plugin callbacks or selection callbacks run under the registry lock. */
            {
                std::lock_guard<std::mutex> lock(this->mutex);
                for (const auto& existing : this->plugins) {
                    if (existing->IsSameImage(*plugin)) {
                        return fail(MC_ERROR_PLUGIN, "Plugin image already loaded", error);
                    }
                }
            }

            const auto identified = plugin->Identify(error);
            if (identified != MC_OK) {
                return identified;
            }
            auto info = plugin->Describe(true);
            {
                std::lock_guard<std::mutex> lock(this->mutex);
                for (const auto& existing : this->descriptions) {
                    if (existing.id == info.id) {
                        return fail(MC_ERROR_PLUGIN, "Duplicate plugin ID", error);
                    }
                }
            }

            if (select) {
                /* selection is user code too; prevent nested lifecycle changes
                from invalidating the transaction before publication. */
                PluginCallState::Scope scope(this->calls);
                try {
                    info.enabled = select(info);
                }
                catch (...) {
                    return fail(MC_ERROR_INTERNAL, "Host plugin selection callback threw an exception", error);
                }
            }

            if (info.enabled) {
                const auto initialized = plugin->Initialize(error);
                if (initialized != MC_OK) {
                    return initialized;
                }
            }

            /* reserve both vectors before publishing either. allocation failure
            rolls back the candidate, including any partially initialized state. */
            {
                std::lock_guard<std::mutex> lock(this->mutex);
                this->descriptions.reserve(this->descriptions.size() + 1);
                if (info.enabled) {
                    this->plugins.reserve(this->plugins.size() + 1);
                    this->plugins.push_back(std::move(plugin));
                }
                this->descriptions.push_back(std::move(info));
            }
            return MC_OK;
        });
    }

    mc_result PluginRegistry::Discover(const std::string& directory, const Selection& select) {
        namespace fs = std::filesystem;
        std::error_code error;
        fs::directory_iterator cursor(fs::u8path(directory), error);
        if (error == std::errc::no_such_file_or_directory) {
            return MC_OK;
        }

        std::vector<std::string> files;
        const fs::directory_iterator end;
        while (!error && cursor != end) {
            const auto path = cursor->path();
            auto extension = pathText(path.extension());
#ifdef _WIN32
            std::transform(extension.begin(), extension.end(), extension.begin(), [](char value) {
                return value >= 'A' && value <= 'Z' ? (char) (value + ('a' - 'A')) : value;
            });
            const bool candidate = extension == ".dll";
#elif defined(__APPLE__)
            const bool candidate = extension == ".dylib";
#else
            const bool candidate = extension == ".so";
#endif
            if (candidate) {
                /* one unreadable entry must not stop iteration through the
                directory. keep candidate errors separate from cursor errors. */
                std::error_code entryError;
                const bool regular = cursor->is_regular_file(entryError);
                if (!entryError && regular) {
                    const auto absolute = fs::absolute(path, entryError);
                    if (!entryError) {
                        files.push_back(pathText(absolute));
                    }
                }
                if (entryError) {
                    this->LogLoadFailure(pathText(path), MC_ERROR_IO, entryError.message());
                }
            }
            if (!error) {
                cursor.increment(error);
            }
        }

        if (error) {
            this->LogLoadFailure(directory, MC_ERROR_IO, error.message());
            return MC_ERROR_IO;
        }
        std::sort(files.begin(), files.end());
        for (const auto& filename : files) {
            mc_error* detail = nullptr;
            const auto status = this->Load(filename, &detail, select);
            const auto destroy = [](mc_error* owned) {
                mc_error_destroy(&owned);
            };
            const std::unique_ptr<mc_error, decltype(destroy)> owned(detail, destroy);
            const auto view = mc_error_get_message(detail);
            if (status != MC_OK) {
                this->LogLoadFailure(
                    filename, status, view.length ? std::string(view.data, view.length) : "Plugin rejected");
            }
        }
        return MC_OK;
    }

    std::vector<PluginRegistry::PluginInfo> PluginRegistry::ListPlugins() const {
        std::lock_guard<std::mutex> lock(this->mutex);
        return this->descriptions;
    }

    const void* PluginRegistry::FindTable(std::string_view pluginId, const char* capabilityId) const {
        std::lock_guard<std::mutex> lock(this->mutex);
        if (this->phase == Phase::Running) {
            for (const auto& plugin : this->plugins) {
                if (plugin->HasId(pluginId)) {
                    const auto table = plugin->FindCapability(capabilityId);
                    return table ? table->api : nullptr;
                }
            }
        }
        return nullptr;
    }

    void PluginRegistry::AssertCanShutdown() const {
        musik::contract::Assert(this->calls.IsOwnerThread(), "engine shutdown on the wrong plugin owner thread");
        musik::contract::Assert(!this->calls.IsActive(), "engine shutdown reentered an active plugin call");
    }

    mc_result PluginRegistry::Shutdown() {
        if (!this->calls.IsOwnerThread()) {
            return MC_ERROR_WRONG_THREAD;
        }
        std::vector<std::unique_ptr<Plugin>> plugins;
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            if (this->phase == Phase::Stopped) {
                return MC_OK;
            }
            /* fail before changing ownership or unloading executing code.
            empty/completed cleanup has no live plugin state to invalidate. */
            musik::contract::Assert(this->phase != Phase::Stopping &&
                    !(this->calls.IsActive() && (this->phase == Phase::Initializing || !this->plugins.empty())),
                "PluginRegistry shutdown reentered an active session");
            this->phase = Phase::Stopping;
            plugins.swap(this->plugins);
        }
        /* callers have quiesced the engine before entering Shutdown. capability
        views do not delay unload; only copied metadata remains valid afterward. */
        PluginCallState::Scope scope(this->calls);
        mc_result status = MC_OK;
        for (const auto& plugin : plugins) {
            const auto stopped = plugin->Stop();
            if (stopped != MC_OK) {
                status = stopped;
            }
        }
        std::lock_guard<std::mutex> lock(this->mutex);
        this->phase = Phase::Stopped;
        return status;
    }
} } }
