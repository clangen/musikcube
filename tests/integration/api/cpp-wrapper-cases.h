#pragma once

#include <musikcore/context.h>
#include <functional>

extern "C" void MC_CALL query_test_wakeup(void*);
extern "C" int query_test_wait_for_wakeup(unsigned count);
extern "C" unsigned query_test_wakeup_count();
extern "C" void query_test_release_wakeup(void);
bool query_test_dispatch_until(mc_context*, const std::function<bool()>&);

namespace wrapper_tests {
    bool checkQuery(const char* directory);
    bool checkSearch(const char* directory);
    bool checkResultWindows(const char* directory);
#ifdef MUSIKCORE_TEST_PRODUCTION_RUNTIME
    bool checkMessageCleanup();
    bool checkPlaybackCommands();
    bool checkRemoteQueryPayloads();
    bool checkBrowseReads(const char* directory);
    bool checkSelectedBackend();
#endif
}
