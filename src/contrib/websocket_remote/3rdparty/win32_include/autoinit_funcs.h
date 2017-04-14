/*
 *  AutoinitFuncs: Automatic Initialization and Deinitialization Functions
 *  Copyright(C) 2014-2017  Karlson2k (Evgeny Grin)
 *
 *  This header is free software; you can redistribute it and / or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This header is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this header; if not, see
 *  <http://www.gnu.org/licenses/>.
 */

/*
   General usage is simple: include this header, declare or define two
   functions with zero parameters (void) and any return type: one for
   initialization and one for deinitialization, add 
   _SET_INIT_AND_DEINIT_FUNCS(FuncInitName, FuncDeInitName) to the code
   and functions will be automatically called during application startup
   and shutdown.
   This is useful for libraries as libraries doesn't have direct access
   to main() functions.
   Example:
   -------------------------------------------------
   #include <stdlib.h>
   #include "autoinit_funcs.h"

   int someVar;
   void* somePtr;

   void libInit(void)
   {
     someVar = 3;
     somePtr = malloc(100);
   }

   void libDeinit(void)
   {
     free(somePtr);
   }

   _SET_INIT_AND_DEINIT_FUNCS(libInit,libDeinit);
   -------------------------------------------------

   If initializer or deinitializer function is not needed, just define
   it as empty function.

   This header should work with GCC, clang, MSVC (2010 or later) and
   SunPro / Sun Studio / Oracle Solaris Studio / Oracle Developer Studio
   compiler.
   Supported C and C++ languages; application, static and dynamic (DLL)
   libraries; non-optimized (Debug) and optimized (Release) compilation
   and linking.

   For more information see header code and comments in code.
 */
#ifndef AUTOINIT_FUNCS_INCLUDED
#define AUTOINIT_FUNCS_INCLUDED 1

/**
* Current version of the header.
* 0x01093001 = 1.9.30-1.
*/
#define AUTOINIT_FUNCS_VERSION 0x01000100

#if defined(__GNUC__)
 /* if possible - check for supported attribute */
#ifdef __has_attribute
#if !__has_attribute(constructor) || !__has_attribute(destructor)
#define _GNUC_ATTR_CONSTR_NOT_SUPPORTED 1
#endif /* !__has_attribute(constructor) || !__has_attribute(destructor) */
#endif /* __has_attribute */
#endif /* __GNUC__ */

/* "_attribute__ ((constructor))" is supported by GCC, clang and
   Sun/Oracle compiler starting from version 12.1. */
#if (defined(__GNUC__) && !defined(_GNUC_ATTR_CONSTR_NOT_SUPPORTED)) || \
    (defined(__SUNPRO_C) && __SUNPRO_C+0 >= 0x5100)

#define GNUC_SET_INIT_AND_DEINIT(FI,FD) \
  void __attribute__ ((constructor)) _GNUC_init_helper_##FI(void) \
    { (void)(FI)(); } \
  void __attribute__ ((destructor)) _GNUC_deinit_helper_##FD(void) \
    { (void)(FD)(); } \
  struct _GNUC_dummy_str_##FI{int i;}

#define _SET_INIT_AND_DEINIT_FUNCS(FI,FD) GNUC_SET_INIT_AND_DEINIT(FI,FD)
#define _AUTOINIT_FUNCS_ARE_SUPPORTED 1

#elif defined (_MSC_FULL_VER) && _MSC_VER+0 >= 1600

/* Make sure that your project/sources define:
   _LIB if building a static library (_LIB is ignored if _CONSOLE is defined);
   _USRDLL if building DLL-library;
   not defined both _LIB and _USRDLL if building an application */

/* Define AUTOINIT_FUNCS_DECLARE_STATIC_REG if you need macro declaration
   for registering static initialization functions even if you building DLL */
/* Define AUTOINIT_FUNCS_FORCE_STATIC_REG if you want to set main macro
   _SET_INIT_AND_DEINIT_FUNCS to static version even if building a DLL*/

/* Stringify macros */
#define _INSTRMACRO(a) #a
#define _STRMACRO(a) _INSTRMACRO(a)

#if !defined(_USRDLL) || defined(AUTOINIT_FUNCS_DECLARE_STATIC_REG)

/* required for atexit() */
#include <stdlib.h>

/* Use "C" linkage for variable to simplify variable decoration */
#ifdef __cplusplus
#define W32_INITVARDECL extern "C"
#else
#define W32_INITVARDECL extern
#endif

/* How variable is decorated by compiler */
#if defined(_M_X64) || defined(_M_AMD64)
#define W32_VARDECORPREFIX
#define W32_DECORVARNAME(v) v
#define W32_VARDECORPEFIXSTR ""
#elif defined(_M_IX86) || defined(_X86_)
#define W32_VARDECORPREFIX _
#define W32_DECORVARNAME(v) _##v
#define W32_VARDECORPEFIXSTR "_"
#else
#error Do not know how to decorate symbols for this architecture
#endif

/* Internal variable prefix (can be any) */
#define W32_INITHELPERVARNAME(f) _initHelperDummy_##f
#define W32_INITHELPERVARNAMEDECORSTR(f) W32_VARDECORPEFIXSTR _STRMACRO(W32_INITHELPERVARNAME(f))

/* Declare section (segment), put variable pointing to init function to chosen segment,
   force linker to include variable to avoid omitting by optimizer */
/* Initialization function must be declared as
   int __cdecl FuncName(void) */
/* Return value is ignored for C++ initializers */
/* For C initializers: startup process is aborted if initializer return non-zero */
#define W32_FPTR_IN_SEG(S,F) \
  __pragma(section(S,long,read)) \
  __pragma(comment(linker, "/INCLUDE:" W32_INITHELPERVARNAMEDECORSTR(F))) \
  W32_INITVARDECL __declspec(allocate(S)) int(__cdecl *W32_INITHELPERVARNAME(F))(void) = &F

/* Section (segment) names for pointers to initializers */
#define W32_SEG_INIT_C_USER   ".CRT$XCU"
#define W32_SEG_INIT_C_LIB    ".CRT$XCL"
#define W32_SEG_INIT_CXX_USER ".CRT$XIU"
#define W32_SEG_INIT_CXX_LIB  ".CRT$XIL"

/* Declare macro for different initializers sections */
/* Macro can be used several times to register several initializers */
/* Once function is registered as initializer, it will be called automatically
   during application startup */
/* "lib" initializers are called before "user" initializers */
/* "C" initializers are called before "C++" initializers */
#define W32_REG_INIT_C_USER(F) W32_FPTR_IN_SEG(W32_SEG_INIT_C_USER,F)
#define W32_REG_INIT_C_LIB(F) W32_FPTR_IN_SEG(W32_SEG_INIT_C_LIB,F)
#define W32_REG_INIT_CXX_USER(F) W32_FPTR_IN_SEG(W32_SEG_INIT_CXX_USER,F)
#define W32_REG_INIT_CXX_LIB(F) W32_FPTR_IN_SEG(W32_SEG_INIT_CXX_LIB,F)

/* Choose main register macro based on language and program type */
/* Assuming that _LIB or _USRDLL is defined for static or DLL-library */
/* Macro can be used several times to register several initializers */
/* Once function is registered as initializer, it will be called automatically
   during application startup */
/* Define AUTOINIT_FUNCS_FORCE_USER_LVL_INIT to register initializers
   at user level even if building library */
#ifdef __cplusplus
#if ((defined(_LIB) && !defined(_CONSOLE)) || defined(_USRDLL)) && !defined(AUTOINIT_FUNCS_FORCE_USER_LVL_INIT)
#define W32_REGISTER_INIT(F) W32_REG_INIT_CXX_LIB(F)
#else  /* ! _LIB && ! _DLL */
#define W32_REGISTER_INIT(F) W32_REG_INIT_CXX_USER(F)
#endif /* ! _LIB && ! _DLL */
#else  /* !__cplusplus*/
#if ((defined(_LIB) && !defined(_CONSOLE)) || defined(_USRDLL)) && !defined(AUTOINIT_FUNCS_FORCE_USER_LVL_INIT)
#define W32_REGISTER_INIT(F) W32_REG_INIT_C_LIB(F)
#else  /* ! _LIB && ! _DLL */
#define W32_REGISTER_INIT(F) W32_REG_INIT_C_USER(F)
#endif /* ! _LIB && ! _DLL */
#endif /* !__cplusplus*/

#else /* _USRDLL */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif /* WIN32_LEAN_AND_MEAN */
/* Required for DllMain */
#include <Windows.h>
#endif /* _USRDLL */


#if !defined(_USRDLL) || defined(AUTOINIT_FUNCS_FORCE_STATIC_REG)
#define W32_SET_INIT_AND_DEINIT(FI,FD) \
  void __cdecl _W32_deinit_helper_##FD(void) \
   { (void)(FD)(); } \
  int __cdecl _W32_init_helper_##FI(void) \
   { (void)(FI)(); atexit(_W32_deinit_helper_##FD); return 0; } \
  W32_REGISTER_INIT(_W32_init_helper_##FI)
#else  /* _USRDLL */

/* If DllMain is already present in code, define AUTOINIT_FUNCS_CALL_USR_DLLMAIN 
   and rename DllMain to usr_DllMain */
#ifndef AUTOINIT_FUNCS_CALL_USR_DLLMAIN
#define W32_SET_INIT_AND_DEINIT(FI,FD) \
  BOOL WINAPI DllMain(HINSTANCE hinst,DWORD reason,LPVOID unused) \
    { if(DLL_PROCESS_ATTACH==reason) {(void)(FI)();} \
      else if(DLL_PROCESS_DETACH==reason) {(void)(FD)();} \
      return TRUE; \
    } struct _W32_dummy_strc_##FI{int i;}
#else  /* AUTOINIT_FUNCS_CALL_USR_DLLMAIN */
#define W32_SET_INIT_AND_DEINIT(FI,FD) \
  BOOL WINAPI usr_DllMain(HINSTANCE hinst,DWORD reason,LPVOID unused); \
  BOOL WINAPI DllMain(HINSTANCE hinst,DWORD reason,LPVOID unused) \
    { if(DLL_PROCESS_ATTACH==reason) {(void)(FI)();} \
      else if(DLL_PROCESS_DETACH==reason) {(void)(FD)();} \
      return usr_DllMain(hinst,reason,unused); \
    } struct _W32_dummy_strc_##FI{int i;}
#endif /* AUTOINIT_FUNCS_CALL_USR_DLLMAIN */
#endif /* _USRDLL */

#define _SET_INIT_AND_DEINIT_FUNCS(FI,FD) W32_SET_INIT_AND_DEINIT(FI,FD)
/* Indicate that automatic initializers/deinitializers are supported */
#define _AUTOINIT_FUNCS_ARE_SUPPORTED 1

#else  /* !__GNUC__ && !_MSC_FULL_VER */

/* Define EMIT_ERROR_IF_AUTOINIT_FUNCS_ARE_NOT_SUPPORTED before inclusion of header to
   abort compilation if automatic initializers/deinitializers are not supported */
#ifdef EMIT_ERROR_IF_AUTOINIT_FUNCS_ARE_NOT_SUPPORTED
#error Compiler/platform don not support automatic calls of user-defined initializer and deinitializer
#endif /* EMIT_ERROR_IF_AUTOINIT_FUNCS_ARE_NOT_SUPPORTED */

/* Do nothing */
#define _SET_INIT_AND_DEINIT_FUNCS(FI,FD)
/* Indicate that automatic initializers/deinitializers are not supported */
#define _AUTOINIT_FUNCS_ARE_NOT_SUPPORTED 1

#endif /* !__GNUC__ && !_MSC_FULL_VER */
#endif /* !AUTOINIT_FUNCS_INCLUDED */
