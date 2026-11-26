#pragma once

/* select the transport at build time; callers use the same HTTP interface. */
#if defined(MUSIKCORE_USE_WINDOWS_HTTP_CLIENT) && !defined(MUSIKCORE_HTTP_TEST_CURL)
#include "HttpClientWindows.h"
#else
#include "HttpClientCurl.h"
#endif
