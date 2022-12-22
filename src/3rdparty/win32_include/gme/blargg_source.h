/* Included at the beginning of library source files, after all other #include lines.
 * Sets up helpful macros and services used in my source code. They don't need
 * module an annoying module prefix on their names since they are defined after
 * all other #include lines. */

#ifndef BLARGG_SOURCE_H
#define BLARGG_SOURCE_H

/* If debugging is enabled, abort program if expr is false. Meant for checking
 * internal state and consistency. A failed assertion indicates a bug in the module.
 * void assert( bool expr ); */
#include <assert.h>

/* If debugging is enabled and expr is false, abort program. Meant for checking
 * caller-supplied parameters and operations that are outside the control of the
 * module. A failed requirement indicates a bug outside the module.
 * void require( bool expr ); */
#undef require
#define require( expr ) assert( expr )

/* Use to provide hints to compiler for optimized code layout in situations where we
 * can almost always expect a conditional to go one way or the other.  Should only be
 * used in situations where an unexpected branch is truly exceptional though! */
#undef likely
#undef unlikely
#ifdef __GNUC__
    #define likely( x ) __builtin_expect(x, 1)
    #define unlikely( x ) __builtin_expect(x, 0)
#else
    #define likely( x ) (x)
    #define unlikely( x ) (x)
#endif

/* Like printf() except output goes to debug log file. Might be defined to do
 * nothing (not even evaluate its arguments).
 * void debug_printf( const char* format, ... ); */
#if defined(__cplusplus) && defined(BLARGG_BUILD_DLL)
    static inline void blargg_dprintf_( const char* fmt_str, ... ) { (void) fmt_str; }
    #undef debug_printf
    #define debug_printf (1) ? (void) 0 : blargg_dprintf_
#endif

/* If enabled, evaluate expr and if false, make debug log entry with source file
 * and line. Meant for finding situations that should be examined further, but that
 * don't indicate a problem. In all cases, execution continues normally. */
#undef check
#define check( expr ) ((void) 0)

/* If expr yields error string, return it from current function, otherwise continue. */
#undef RETURN_ERR
#define RETURN_ERR( expr ) do {                         \
		blargg_err_t blargg_return_err_ = (expr);               \
		if ( blargg_return_err_ ) return blargg_return_err_;    \
	} while ( 0 )

/* If ptr is 0, return out of memory error string. */
#undef CHECK_ALLOC
#define CHECK_ALLOC( ptr ) do { if ( (ptr) == 0 ) return "Out of memory"; } while ( 0 )

/* TODO: good idea? bad idea? */
#undef byte
#define byte byte_
typedef unsigned char byte;

/* Setup compiler defines useful for exporting required public API symbols in gme.cpp */
#ifndef BLARGG_EXPORT
    #if defined (_WIN32)
        #if defined(BLARGG_BUILD_DLL)
            #define BLARGG_EXPORT __declspec(dllexport)
        #else
            #define BLARGG_EXPORT /* Leave blank: friendly with both static and shared linking */
        #endif
    #elif defined (LIBGME_VISIBILITY) && defined(__cplusplus)
        #define BLARGG_EXPORT __attribute__((visibility ("default")))
    #else
        #define BLARGG_EXPORT
    #endif
#endif

/* deprecated */
#define BLARGG_CHECK_ALLOC CHECK_ALLOC
#define BLARGG_RETURN_ERR RETURN_ERR

/* BLARGG_SOURCE_BEGIN: If defined, #included, allowing redefition of debug_printf and check */
#ifdef BLARGG_SOURCE_BEGIN
	#include BLARGG_SOURCE_BEGIN
#endif

#endif
