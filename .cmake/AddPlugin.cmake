# C SDK plugins expose mc_plugin_get_api. the host reaches individual
# factories through that function, so their symbols can stay private.
function(musikcube_configure_c_plugin target)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_compile_definitions(${target} PRIVATE MC_BUILDING_PLUGIN)
    set_target_properties(${target} PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
    musikcube_restrict_exports(${target} mc_plugin_get_api)
endfunction()

macro(add_plugin plugin_dir plugin_name)
    add_subdirectory(${plugin_dir})
    get_property(PLUGIN_DISABLED GLOBAL PROPERTY "DISABLE_${plugin_name}")
    if (NOT PLUGIN_DISABLED STREQUAL "true")
        # C SDK targets already have their export list. other plugins expose
        # GetPlugin and the additional functions named by their caller.
        get_property(has_exports TARGET ${plugin_name} PROPERTY MUSIKCUBE_EXPORT_ALLOWLIST SET)
        if(NOT has_exports)
            musikcube_restrict_exports(${plugin_name} GetPlugin ${ARGN})
        endif()
        # Standalone plugins resolve the core in the parent and vendors in lib.
        if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND BUILD_STANDALONE)
            set_target_properties(${plugin_name} PROPERTIES INSTALL_RPATH "$ORIGIN:$ORIGIN/..:$ORIGIN/../lib")
        endif()
        message(STATUS "[add-plugin] ${BoldGreen}added ${plugin_name}${ColorReset}")
        add_dependencies(musikcube ${plugin_name})
        add_dependencies(musikcubed ${plugin_name})
    else()
        message(STATUS "[add-plugin] ${BoldRed}skipped ${plugin_name}${ColorReset}")
    endif()
endmacro(add_plugin)

macro(disable_plugin plugin_name)
    set(PROPERTY_NAME "DISABLE_${plugin_name}")
    set_property(GLOBAL PROPERTY ${PROPERTY_NAME} "true")
    get_property(PLUGIN_DISABLED GLOBAL PROPERTY "DISABLE_${plugin_name}")
endmacro(disable_plugin)
