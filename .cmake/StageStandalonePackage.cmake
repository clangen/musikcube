# use the audited runtime binaries in the package and retain CMake's install layout.
set(runtime "${CPACK_TEMPORARY_DIRECTORY}${CPACK_INSTALL_PREFIX}/share/musikcube")
if (NOT IS_DIRECTORY "${runtime}" OR NOT IS_DIRECTORY "${MUSIKCUBE_STAGED_ARCHIVE}")
  message(FATAL_ERROR "Missing standalone package runtime or audited archive")
endif()

foreach(entry IN ITEMS musikcube musikcubed libmusikcore.so lib plugins)
  file(REMOVE_RECURSE "${runtime}/${entry}")
  file(COPY "${MUSIKCUBE_STAGED_ARCHIVE}/${entry}" DESTINATION "${runtime}")
endforeach()
