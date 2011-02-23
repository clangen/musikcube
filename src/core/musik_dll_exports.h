// Contents of DLLDefines.h
#ifndef _musik_DLLDEFINES_H_
#define _musik_DLLDEFINES_H_

/* Cmake will define S on Windows when it
configures to build a shared library. If you are going to use
another build system on windows or create the visual studio
projects by hand you need to define S when
building a DLL on windows.
*/
// We are using the Visual Studio Compiler and building Shared libraries

#if defined (_WIN32) 
  #if defined(S)
    #define   __declspec(dllexport)
  #else
    #define   __declspec(dllimport)
  #endif /* S */
#else /* defined (_WIN32) */
 #define 
#endif

#endif /* _musik_DLLDEFINES_H_ */
