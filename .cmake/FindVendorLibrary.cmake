macro(find_vendor_library target_var library_name)
    find_library(${target_var} NAMES ${library_name} PATHS ${VENDOR_LINK_DIRECTORIES} NO_DEFAULT_PATH)
    message(STATUS "[find-vendor-library] '${library_name}' resolved to '${${target_var}}'")
endmacro(find_vendor_library)

macro(add_vendor_includes target_project)
    target_include_directories(${target_project} PRIVATE BEFORE ${VENDOR_INCLUDE_DIRECTORIES})
    message(STATUS "[add-vendor-includes] adding vendor includes to '${target_project}'")
endmacro(add_vendor_includes)