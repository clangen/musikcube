#pragma once

#include <musikcore/plugin.h>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace musik { namespace core { namespace ffi {

    /* owns one provider session. lifecycle stays on its constructing thread;
    capability acquisition/invocation may run on workers. shutdown retires
    registration and waits for every lease/call before unloading. callers must
    release leases before shutdown, or arrange their release on another thread.
    this host is noncopyable/nonmovable; use it at a runtime ownership boundary. */
    class ProviderHost {
        private:
            struct Module;
            struct Lease;

        public:
            using Logger = std::function<void(mc_log_level, mc_string_view, mc_string_view)>;

            /* copyable leases share one reference; moving transfers that reference.
            invoke scopes table access to an admitted call and keeps the native image
            mapped even if the caller resets its own lease inside the operation.
            never retain the table after Invoke returns or use this on an audio thread. */
            class Capability {
                public:
                    Capability() = default;
                    Capability(const Capability&) = default;
                    Capability(Capability&&) noexcept = default;
                    ~Capability() = default;

                    Capability& operator=(const Capability&) = default;
                    Capability& operator=(Capability&&) noexcept = default;

                    explicit operator bool() const noexcept { return bool(this->lease); }
                    mc_result Invoke(const std::function<mc_result(const void*, uint32_t, uint32_t)>& operation) const;

                private:
                    friend class ProviderHost;
                    explicit Capability(std::shared_ptr<Lease> lease)
                    : lease(std::move(lease)) {}

                    std::shared_ptr<Lease> lease;
            };

            /* logging may run on provider workers; consume text during the call
            and synchronize mutable state owned by the supplied sink. */
            explicit ProviderHost(Logger logger = {});
            ~ProviderHost();
            ProviderHost(const ProviderHost&) = delete;
            ProviderHost(ProviderHost&&) = delete;

            ProviderHost& operator=(const ProviderHost&) = delete;
            ProviderHost& operator=(ProviderHost&&) = delete;

            /* unsupported means no C entry point; SDK 23 discovery is independent. */
            mc_result Load(const std::string& filename, mc_error** error = nullptr);
            Capability Acquire(const std::string& pluginId, const std::string& capabilityId);
            mc_result Shutdown();

        private:
            Logger logger;
            const std::thread::id owner;
            std::mutex mutex;
            bool stopping = false;
            std::vector<std::shared_ptr<Module>> modules;
    };

} } }
