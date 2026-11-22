/* storage declarations must not reserve the global query name. third-party
ADL code such as Asio uses that spelling when looking for callable functions. */
#include <musikcore/ffi/consumer/library-storage.h>

/* a declaration catches namespace-alias pollution without networking dependencies. */
void query();
