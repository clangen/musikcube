#pragma once

#include <core/sdk/config.h>

#ifdef WIN32
#include <shlwapi.h>
#else
  #define __forceinline inline
#endif

#include <math.h>
