#pragma once

#include <common/DeleteDefaults.h>
#include <cstdio>
#include <cstdlib>
#include <exception>

/* a fatal child must abort without unwinding owners or invoking the embedding
application's terminate handler. normal return and either detour leave evidence.
non-copyable and non-movable; keep it outside the owners under test. */
class FatalScope {
public:
    /* make an accidental terminate-handler call an unmistakable child failure. */
    FatalScope() {
#ifdef _MSC_VER
        /* let the CRT exit with code 3 instead of opening a crash dialog or
        invoking Windows Error Reporting during a death test. */
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
        std::set_terminate([] {
            std::_Exit(TerminateHandlerInvoked);
        });
    }

    /* leave evidence if the fatal operation returns or unwinds its owners. */
    ~FatalScope() {
        std::fputs("fatal-test: stack unwound\n", stderr);
    }

    MUSIK_DELETE_COPY_AND_MOVE(FatalScope);

private:
    static constexpr int TerminateHandlerInvoked = 81;
};
