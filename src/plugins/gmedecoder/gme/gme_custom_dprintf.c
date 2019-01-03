#include "gme_custom_dprintf.h"

#ifdef CUSTOM_DPRINTF_FUNCTION
gme_custom_dprintf_callback gme_custom_dprintf = 0;
#endif