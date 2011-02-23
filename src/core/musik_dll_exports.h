// Contents of DLLDefines.h
#ifndef _musik_DLLDEFINES_H_
#define _musik_DLLDEFINES_H_

/* Cmake will define musik_EXPORTS on Windows when it
configures to build a shared library. If you are going to use
another build system on windows or create the visual studio
projects by hand you need to define musik_EXPORTS when
building a DLL on windows.
*/
// We are using the Visual Studio Compiler and building Shared libraries

#if defined (_WIN32) 
  #if defined(musik_EXPORTS)
    #define  MUSIK_EXPORT __declspec(dllexport)
  #else
    #define  MUSIK_EXPORT __declspec(dllimport)
  #endif /* musik_EXPORTS */
#else /* defined (_WIN32) */
 #define MUSIK_EXPORT
#endif

#endif /* _musik_DLLDEFINES_H_ */
