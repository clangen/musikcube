#ifndef GME_CUSTOM_DPRINTF_H
#define GME_CUSTOM_DPRINTF_H


#ifdef CUSTOM_DPRINTF_FUNCTION


#include <stdarg.h>


#ifdef _cplusplus
extern "C" {
#endif


typedef void (*gme_custom_dprintf_callback)( const char * fmt, va_list vl );
extern gme_custom_dprintf_callback gme_custom_dprintf;


#ifdef _cplusplus
}
#endif


#endif


#endif
