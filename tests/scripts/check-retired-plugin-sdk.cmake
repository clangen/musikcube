# The old C++ plugin boundary has no supported callers after the C cutover.
file(GLOB retired_headers "${SOURCE}/src/musikcore/sdk/I*.h")
if(retired_headers)
  message(FATAL_ERROR "Retired SDK interfaces reintroduced: ${retired_headers}")
endif()
foreach(path IN ITEMS
    "${SOURCE}/src/musikcore/plugin/PluginFactory.h"
    "${SOURCE}/src/musikcore/plugin/PluginFactory.cpp"
    "${SOURCE}/src/musikcore/audio/IStream.h"
    "${SOURCE}/src/common/LegacyConstants.h")
  if(EXISTS "${path}")
    message(FATAL_ERROR "Retired plugin implementation reintroduced: ${path}")
  endif()
endforeach()
file(GLOB_RECURSE sources
  "${SOURCE}/src/musikcore/*.h" "${SOURCE}/src/musikcore/*.hpp" "${SOURCE}/src/musikcore/*.cpp"
  "${SOURCE}/src/plugins/*.h" "${SOURCE}/src/plugins/*.hpp" "${SOURCE}/src/plugins/*.cpp"
  "${SOURCE}/src/plugins/*.c" "${SOURCE}/src/plugins/*.mm")
foreach(path IN LISTS sources)
  file(READ "${path}" contents)
  if(contents MATCHES "(^|[^A-Za-z0-9_])(GetPlugin|SetEnvironment|SetPreferences|SetDebug)[ \t\r\n]*\\(")
    message(FATAL_ERROR "Retired plugin entry point referenced in ${path}")
  endif()
endforeach()
