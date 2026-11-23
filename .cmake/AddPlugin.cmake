macro(add_plugin plugin_dir plugin_name)
    add_subdirectory(${plugin_dir})
    get_property(PLUGIN_DISABLED GLOBAL PROPERTY "DISABLE_${plugin_name}")
    if (NOT PLUGIN_DISABLED STREQUAL "true")
        # MUSIKCUBE_PLUGIN_ENTRY names the target's exported entry point.
        # when unset, it defaults to GetPlugin. set it in the plugin's
        # CMakeLists.txt before add_subdirectory returns.
        get_target_property(plugin_entry ${plugin_name} MUSIKCUBE_PLUGIN_ENTRY)
        if(NOT plugin_entry)
            set(plugin_entry GetPlugin)
        endif()
        # ARGN contains additional exports named at the add_plugin call.
        # mc_plugin_get_api returns factory tables, so it needs no extra exports.
        musikcube_restrict_exports(${plugin_name} ${plugin_entry} ${ARGN})
        # Standalone plugins resolve vendor libraries in the sibling lib directory.
        if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND BUILD_STANDALONE)
            set_target_properties(${plugin_name} PROPERTIES INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib")
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
