prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=@CMAKE_INSTALL_PREFIX@
libdir=@LIB_INSTALL_DIR@
includedir=@INCLUDE_INSTALL_DIR@

Name: TagLib
Description: Audio meta-data library
Requires: 
Version: @TAGLIB_LIB_VERSION_STRING@
Libs: -L${libdir} -ltag @ZLIB_LIBRARIES_FLAGS@
Cflags: -I${includedir}/taglib
