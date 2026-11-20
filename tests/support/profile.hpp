#pragma once
#include "profile.h"
#include <cstdio>
#ifdef __APPLE__
#include <dlfcn.h>
#endif

/* Neither copyable nor movable. Profiles one sequential host operation and all
   its producer threads. Uses an injected test dylib only when explicitly enabled. */
class ProfilePhase {
public:
    /* Begin optional dyld profiling for this phase; caller-only mode excludes worker allocations. */
    explicit ProfilePhase(const char* name, bool callingThreadOnly = false) : name(name) {
        if (!name) return;
#ifdef __APPLE__
        this->begin = reinterpret_cast<Begin>(dlsym(RTLD_DEFAULT, callingThreadOnly
            ? "musikcore_test_profile_begin_thread" : "musikcore_test_profile_begin"));
        this->end = reinterpret_cast<End>(dlsym(RTLD_DEFAULT, "musikcore_test_profile_end"));
        if (this->begin && this->end) {
            this->begin();
            this->active = true;
        }
#endif
    }
    /* End an unfinished phase during normal teardown or exception unwinding. */
    ~ProfilePhase() { if (this->active) this->Finish(); }

    ProfilePhase(const ProfilePhase&) = delete;
    ProfilePhase& operator=(const ProfilePhase&) = delete;
    ProfilePhase(ProfilePhase&&) = delete;
    ProfilePhase& operator=(ProfilePhase&&) = delete;

    /* Stop counting before printing results and return the measured allocation/copy totals. */
    test_profile_counts Finish() {
        test_profile_counts counts{};

        if (this->active) {
            this->end(&counts);
            this->active = false;
            std::printf("profile %s: allocations=%llu requested_bytes=%llu dynamic_copy_bytes=%llu\n",
                this->name, static_cast<unsigned long long>(counts.allocations),
                static_cast<unsigned long long>(counts.requested_bytes),
                static_cast<unsigned long long>(counts.copied_bytes));
        }

        return counts;
    }

private:
    using Begin = void(*)();
    using End = void(*)(test_profile_counts*);
    const char* name;
    Begin begin = nullptr;
    End end = nullptr;
    bool active = false;
};
