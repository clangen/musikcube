if(CMAKE_VERSION VERSION_LESS 3.21)
  message(FATAL_ERROR "The Windows build requires CMake 3.21 or newer.")
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "Use cmake --preset windows-x64 to generate into a separate build directory.")
endif()
if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
    (CMAKE_VS_PLATFORM_NAME AND NOT CMAKE_VS_PLATFORM_NAME STREQUAL "x64"))
  message(FATAL_ERROR "Configure the Windows build with the MSVC x64 toolchain.")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/WindowsSources.cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(MUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin64" CACHE PATH
  "Production runtime directory; test builds can select a private directory under their build root")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")

cmake_policy(SET CMP0069 NEW)
include(CheckIPOSupported)
check_ipo_supported(LANGUAGES C CXX)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

set(windows_vendor "${PROJECT_SOURCE_DIR}/vendor")
set(windows_vendor_lib "${windows_vendor}/win64/lib")
set(windows_assets "${PROJECT_SOURCE_DIR}/src/musikcube/data")

function(windows_vendor_library name release)
  add_library(vendor::${name} UNKNOWN IMPORTED)
  if(ARGC GREATER 2)
    set_target_properties(vendor::${name} PROPERTIES
      IMPORTED_LOCATION "${windows_vendor_lib}/${release}.lib"
      IMPORTED_LOCATION_DEBUG "${windows_vendor_lib}/${ARGV2}.lib")
    set(files "${release}.lib" "${ARGV2}.lib")
  else()
    set_target_properties(vendor::${name} PROPERTIES
      IMPORTED_LOCATION "${windows_vendor_lib}/${release}.lib")
    set(files "${release}.lib")
  endif()
  foreach(file IN LISTS files)
    if(NOT EXISTS "${windows_vendor_lib}/${file}")
      message(FATAL_ERROR "Missing ${file}. Run node script/build-vendor-libraries.js first.")
    endif()
  endforeach()
endfunction()

windows_vendor_library(curl libcurl)
windows_vendor_library(ssl libssl)
windows_vendor_library(crypto libcrypto)
windows_vendor_library(lame libmp3lame)
windows_vendor_library(openmpt libopenmpt)
windows_vendor_library(gme gme gmeD)
windows_vendor_library(taglib tag tagD)
windows_vendor_library(zlib zlibstatic zlibstaticd)
windows_vendor_library(microhttpd libmicrohttpd libmicrohttpdD)
foreach(component avcodec avformat avutil swresample)
  windows_vendor_library(${component} ${component}-musikcube)
endforeach()
set(windows_ffmpeg vendor::avcodec vendor::avformat vendor::avutil vendor::swresample)

add_library(musikcube_windows INTERFACE)
target_include_directories(musikcube_windows INTERFACE
  "${PROJECT_SOURCE_DIR}/include"
  "${windows_vendor}/include"
  "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/src/3rdparty/include"
  "${PROJECT_SOURCE_DIR}/src/3rdparty/win32_include"
  "${PROJECT_SOURCE_DIR}/src/3rdparty/asio/asio/include")
target_compile_definitions(musikcube_windows INTERFACE
  WIN32 UNICODE _UNICODE _SCL_SECURE_NO_WARNINGS
  BOOST_DATE_TIME_NO_LIB BOOST_REGEX_NO_LIB ASIO_STANDALONE
  _WEBSOCKETPP_CPP11_TYPE_TRAITS_ _WEBSOCKETPP_CPP11_RANDOM_DEVICE_)
target_compile_options(musikcube_windows INTERFACE
  "$<$<COMPILE_LANGUAGE:C,CXX>:/bigobj;/MP>"
  "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Oi;/Gy>")
target_link_options(musikcube_windows INTERFACE "$<$<CONFIG:Release>:/OPT:REF;/OPT:ICF>")

set(pdcurses_sources ${windows_thirdparty_sources})
list(FILTER pdcurses_sources INCLUDE REGEX "/pdcurses/")
list(FILTER windows_thirdparty_sources EXCLUDE REGEX "/pdcurses/|/wcwidth\\.c$")

add_library(musikcube_thirdparty STATIC ${windows_thirdparty_sources})
target_link_libraries(musikcube_thirdparty PRIVATE musikcube_windows)
target_include_directories(musikcube_thirdparty PRIVATE src/3rdparty/include/sqlite)
target_compile_definitions(musikcube_thirdparty PRIVATE SQLITE_THREADSAFE _CRT_SECURE_NO_DEPRECATE)
set_source_files_properties(src/3rdparty/src/sqlean/unicode/extension.c
  PROPERTIES COMPILE_DEFINITIONS SQLITE_CORE)

add_library(pdcurses_gui STATIC ${pdcurses_sources})
add_library(pdcurses_console STATIC ${pdcurses_sources})
foreach(target pdcurses_gui pdcurses_console)
  target_link_libraries(${target} PRIVATE musikcube_windows)
  target_compile_definitions(${target} PRIVATE PDC_DISABLE_MULTIBYTE PDC_FORCE_UTF8 PDC_WIDE)
endforeach()
target_compile_definitions(pdcurses_gui PRIVATE PDCURSES_WINGUI)
target_compile_definitions(pdcurses_console PRIVATE PDCURSES_WINCON)

add_library(musikcore_objects OBJECT ${musikcore_sources})
target_compile_features(musikcore_objects PRIVATE cxx_std_20)
musikcore_add_consumer_api(musikcore_objects "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/consumer/remote-session.cpp")
target_include_directories(musikcore_objects PRIVATE src/musikcore)
target_link_libraries(musikcore_objects PRIVATE musikcube_windows)
target_compile_definitions(musikcore_objects PRIVATE _CRT_SECURE_NO_DEPRECATE)
target_precompile_headers(musikcore_objects PRIVATE src/musikcore/pch.hpp)

set(windows_core_libraries musikcube_thirdparty Threads::Threads
  vendor::curl vendor::ssl vendor::crypto shell32 ws2_32 wldap32)
add_library(musikcore SHARED $<TARGET_OBJECTS:musikcore_objects>)
set_target_properties(musikcore PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS OFF)
target_include_directories(musikcore PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(musikcore INTERFACE MC_USING_SHARED)
target_link_libraries(musikcore PRIVATE ${windows_core_libraries})

if(BUILD_TESTING)
  # Private implementation fixtures use the same objects as the DLL.
  add_library(musikcore_internal STATIC $<TARGET_OBJECTS:musikcore_objects>)
  # Fixtures include private engine headers such as Player.h, which owns C++20 threads.
  target_compile_features(musikcore_internal INTERFACE cxx_std_20)
  target_include_directories(musikcore_internal PUBLIC "${PROJECT_SOURCE_DIR}/include")
  target_link_libraries(musikcore_internal PRIVATE ${windows_core_libraries})
endif()

# Use one runtime target for both frontends and for the C SDK demo.
file(GLOB windows_dlls CONFIGURE_DEPENDS "${windows_vendor}/win64/dll/*/*.dll")
if(NOT windows_dlls)
  message(FATAL_ERROR "Missing Windows vendor DLLs. Run node script/build-vendor-libraries.js first.")
endif()
add_custom_target(musikcube_runtime
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${windows_dlls}
          "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
          "${windows_assets}/themes" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/themes"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
          "${windows_assets}/locales" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/locales"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
          "${windows_assets}/fonts" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fonts"
  DEPENDS ${windows_dlls}
  VERBATIM)

function(windows_project_sources target directory)
  file(GLOB_RECURSE headers CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/${directory}/*.h"
    "${PROJECT_SOURCE_DIR}/${directory}/*.hpp")
  target_sources(${target} PRIVATE ${headers})
  get_target_property(sources ${target} SOURCES)
  source_group(TREE "${PROJECT_SOURCE_DIR}/src" FILES ${sources})
endfunction()

function(windows_plugin target)
  cmake_parse_arguments(PLUGIN "" "DIRECTORY" "" ${ARGN})
  if(NOT PLUGIN_DIRECTORY)
    set(PLUGIN_DIRECTORY "${target}")
  endif()
  list(TRANSFORM PLUGIN_UNPARSED_ARGUMENTS PREPEND "src/plugins/${PLUGIN_DIRECTORY}/")
  add_library(${target} MODULE ${PLUGIN_UNPARSED_ARGUMENTS})
  target_compile_features(${target} PRIVATE cxx_std_20)
  windows_project_sources(${target} "src/plugins/${PLUGIN_DIRECTORY}")
  target_link_libraries(${target} PRIVATE musikcube_windows)
  set_target_properties(${target} PROPERTIES
    FOLDER plugins
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins")
  add_dependencies(musikcube_runtime ${target})
  install(TARGETS ${target} RUNTIME DESTINATION bin/plugins COMPONENT musikcorePlugins
    LIBRARY DESTINATION bin/plugins COMPONENT musikcorePlugins)
endfunction()

windows_plugin(cddadecoder CddaStream.cpp CddaPcmDecoder.cpp CddaIndexer.cpp CddaMetadata.cpp
  CddaMediaMonitor.cpp plugin.cpp)
musikcube_configure_c_plugin(cddadecoder)
target_link_libraries(cddadecoder PRIVATE bcrypt vendor::curl)

windows_plugin(directsoundout DirectSoundOutput.cpp DirectSoundDevices.cpp plugin.cpp)
musikcube_configure_c_plugin(directsoundout)
target_compile_features(directsoundout PRIVATE cxx_std_20)
target_link_libraries(directsoundout PRIVATE dsound dxguid)

windows_plugin(ffmpegdecoder FfmpegDecoder.cpp plugin.cpp)
musikcube_configure_c_plugin(ffmpegdecoder)
target_link_libraries(ffmpegdecoder PRIVATE ${windows_ffmpeg})

windows_plugin(gmedecoder GmeDecoder.cpp plugin.cpp IndexerPlugin.cpp GmeIndexerSource.cpp)
musikcube_configure_c_plugin(gmedecoder)
target_link_libraries(gmedecoder PRIVATE vendor::gme vendor::zlib)
target_compile_definitions(gmedecoder PRIVATE NOMINMAX)

windows_plugin(httpdatastream HttpDataStream.cpp httpdatastream_plugin.cpp LruDiskCache.cpp)
musikcube_configure_c_plugin(httpdatastream)
target_compile_features(httpdatastream PRIVATE cxx_std_20)
target_link_libraries(httpdatastream PRIVATE shlwapi vendor::curl)

windows_plugin(libopenmptdecoder OpenMptDecoder.cpp plugin.cpp Utility.cpp IndexerPlugin.cpp OpenMptIndexerSource.cpp)
musikcube_configure_c_plugin(libopenmptdecoder)
target_compile_definitions(libopenmptdecoder PRIVATE LIBOPENMPT_USE_DLL)
target_link_libraries(libopenmptdecoder PRIVATE vendor::openmpt)

windows_plugin(nullout plugin.cpp)
musikcube_configure_c_plugin(nullout)
target_link_libraries(nullout PRIVATE Threads::Threads)
target_compile_features(nullout PRIVATE cxx_std_20)

windows_plugin(server BlockingTranscoder.cpp HttpServer.cpp main.cpp ServerControls.cpp ServerMedia.cpp Snapshots.cpp
  Transcoder.cpp TranscodingAudioDataStream.cpp Util.cpp WebSocketServer.cpp)
musikcube_configure_c_plugin(server)
target_compile_features(server PRIVATE cxx_std_20)
target_compile_definitions(server PRIVATE MHD_W32LIB BUILDING_MHD_LIB)
target_include_directories(server PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(server PRIVATE musikcore vendor::zlib vendor::microhttpd ws2_32)

windows_plugin(stockencoders FfmpegEncoder.cpp LameEncoder.cpp main.cpp)
musikcube_configure_c_plugin(stockencoders)
target_link_libraries(stockencoders PRIVATE vendor::lame ${windows_ffmpeg})

windows_plugin(supereqdsp SuperEqDsp.cpp supereqdsp_plugin.cpp supereq/Equ.cpp supereq/Fftsg_fl.c)
musikcube_configure_c_plugin(supereqdsp)

windows_plugin(taglib_plugin taglib_plugin.cpp TaglibMetadataReader.cpp)
musikcube_configure_c_plugin(taglib_plugin)
target_compile_definitions(taglib_plugin PRIVATE TAGLIB_STATIC)
target_compile_options(taglib_plugin PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:/EHa>")
target_link_libraries(taglib_plugin PRIVATE vendor::taglib vendor::zlib)

windows_plugin(wasapiout WasapiOutput.cpp WasapiDevices.cpp plugin.cpp)
musikcube_configure_c_plugin(wasapiout)
target_compile_features(wasapiout PRIVATE cxx_std_20)
target_link_libraries(wasapiout PRIVATE winmm)

windows_plugin(win32gdivis GdiVis.cpp MemoryDC.cpp)
musikcube_configure_c_plugin(win32gdivis)
target_compile_definitions(win32gdivis PRIVATE "$<$<CONFIG:Debug>:DEBUG>")
target_link_libraries(win32gdivis PRIVATE shlwapi)

windows_plugin(win32globalhotkeys win32globalhotkeys_plugin.cpp)
musikcube_configure_c_plugin(win32globalhotkeys)
target_link_libraries(win32globalhotkeys PRIVATE winmm)

foreach(target ffmpegdecoder libopenmptdecoder server stockencoders)
  target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
endforeach()

foreach(target server stockencoders)
  target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/sdl>")
endforeach()

foreach(target directsoundout gmedecoder nullout supereqdsp taglib_plugin
    wasapiout win32gdivis win32globalhotkeys)
  target_link_options(${target} PRIVATE /SUBSYSTEM:WINDOWS)
endforeach()

add_library(musikcube_ui_runtime OBJECT
  src/common/runtime/Message.cpp src/common/runtime/MessageQueue.cpp src/3rdparty/src/wcwidth.c)
target_link_libraries(musikcube_ui_runtime PRIVATE musikcube_windows)

add_executable(musikcube WIN32 ${windows_app_sources})
add_executable(musikcube-cmd ${windows_app_sources})
target_compile_definitions(musikcube PRIVATE PDCURSES_WINGUI)
target_compile_definitions(musikcube-cmd PRIVATE PDCURSES_WINCON)
target_link_libraries(musikcube PRIVATE pdcurses_gui)
target_link_libraries(musikcube-cmd PRIVATE pdcurses_console)
foreach(target musikcube musikcube-cmd)
  target_compile_features(${target} PRIVATE cxx_std_20)
  windows_project_sources(${target} src/musikcube)
  target_include_directories(${target} PRIVATE src/musikcube src/musikcube/cursespp)
  target_precompile_headers(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${PROJECT_SOURCE_DIR}/src/musikcube/stdafx.h>")
  target_link_libraries(${target} PRIVATE musikcube_windows musikcore musikcube_ui_runtime vendor::curl
    dwmapi windowsapp pdh psapi ws2_32 wldap32 comctl32 winmm)
  add_dependencies(${target} musikcube_runtime)
endforeach()

if(BUILD_TESTING)
  add_test(NAME musikcube.integration.windows_core_dll COMMAND "${CMAKE_COMMAND}"
    "-DLINKER=${CMAKE_LINKER}" "-DLIBRARY=$<TARGET_FILE:musikcore>"
    "-DGUI=$<TARGET_FILE:musikcube>" "-DCONSOLE=$<TARGET_FILE:musikcube-cmd>"
    "-DALLOWLIST=${PROJECT_SOURCE_DIR}/src/musikcore/ffi/exports.txt"
    -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-windows-core-dll.cmake")
  set_tests_properties(musikcube.integration.windows_core_dll PROPERTIES
    LABELS "integration;musikcube;abi;contract")
  add_dependencies(musikcube_tests musikcube musikcube-cmd)

  add_executable(musikcube_conpty_host tests/ui/support/windows-conpty.cpp)
  set_target_properties(musikcube_conpty_host PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/ui/bin")
  add_test(NAME musikcube.ui.windows_console COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/ui/scenarios/windows-console.js"
    "$<TARGET_FILE:musikcube_conpty_host>" "$<TARGET_FILE:musikcube-cmd>"
    "$<TARGET_FILE:musikcube_seed_library>" "${CMAKE_BINARY_DIR}")
  set_tests_properties(musikcube.ui.windows_console PROPERTIES LABELS "ui;musikcube;terminal" TIMEOUT 30)
  add_dependencies(musikcube_ui_tests musikcube_conpty_host musikcube-cmd musikcube_seed_library)
  add_dependencies(musikcube_tests musikcube_ui_tests)
endif()

windows_project_sources(musikcore_objects src/musikcore)

set(MUSIKCORE_INSTALL_RUNTIME_DIR bin)
include("${CMAKE_CURRENT_LIST_DIR}/InstallMusikcoreSdk.cmake")
install(FILES ${windows_dlls} DESTINATION bin COMPONENT musikcoreSDK)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT musikcube)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
