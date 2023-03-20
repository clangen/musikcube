macro(find_vendor_library target_var library_name)
    find_library(${target_var} NAMES ${library_name} PATHS ${VENDOR_LINK_DIRECTORIES} NO_DEFAULT_PATH)
    message(STATUS "[find-vendor-library] '${library_name}' resolved to '${${target_var}}'")
endmacro(find_vendor_library)
