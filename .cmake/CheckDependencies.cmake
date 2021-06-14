function(ensure_library_exists libname)
    get_property(ALL_LINK_DIRS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY LINK_DIRECTORIES)
    foreach(CURRENT_LINK_DIR ${ALL_LINK_DIRS})
        unset(__TEMP_ENSURE_LIBRARY CACHE)
        find_library(
            __TEMP_ENSURE_LIBRARY
            NAMES ${libname}
            PATHS ${CURRENT_LINK_DIR})
        if(NOT __TEMP_ENSURE_LIBRARY)
            #message(STATUS "[check-dependencies] ${libname} not found")
        else()
            message(STATUS "[check-dependencies] ${libname} found at ${__TEMP_ENSURE_LIBRARY}")
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "\n\n[check-dependencies] ${libname} not found\n\n")
endfunction(ensure_library_exists)
