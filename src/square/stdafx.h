#pragma once

#ifdef WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include "config.h"
    #define PDC_WIDE
#else
    #include <unistd.h>
#endif

typedef __int64 int64;

