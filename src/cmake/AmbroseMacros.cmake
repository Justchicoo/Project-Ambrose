# Project Ambrose by Imjustchico
# Helpers that build a library or executable from every source file under a folder, copy conf.dist files, and install executables with the DLLs they load and the MariaDB client plugins beside them.
function(ambrose_collect_include_dirs root out)
    set(dirs "${root}")
    file(GLOB_RECURSE children LIST_DIRECTORIES true "${root}/*")
    foreach(child IN LISTS children)
        if(IS_DIRECTORY "${child}")
            list(APPEND dirs "${child}")
        endif()
    endforeach()
    set(${out} "${dirs}" PARENT_SCOPE)
endfunction()

function(ambrose_collect_sources root out)
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS
        "${root}/*.cpp"
        "${root}/*.h")
    set(${out} "${sources}" PARENT_SCOPE)
endfunction()

function(ambrose_add_library name root)
    ambrose_collect_sources("${root}" sources)
    add_library(${name} STATIC ${sources})
    ambrose_collect_include_dirs("${root}" include_dirs)
    target_include_directories(${name} PUBLIC ${include_dirs})
    target_link_libraries(${name} PUBLIC ambrose-compile-options)
endfunction()

function(ambrose_add_executable name root)
    ambrose_collect_sources("${root}" sources)
    add_executable(${name} ${sources})
    ambrose_collect_include_dirs("${root}" include_dirs)
    target_include_directories(${name} PRIVATE ${include_dirs})
    target_link_libraries(${name} PRIVATE ambrose-compile-options)
endfunction()

function(ambrose_copy_conf_dist target)
    file(GLOB dist_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.conf.dist")
    if(NOT dist_files)
        return()
    endif()
    add_custom_target(${target}-conf-dist
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${target}>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${dist_files} "$<TARGET_FILE_DIR:${target}>"
        DEPENDS ${dist_files}
        SOURCES ${dist_files}
        VERBATIM)
    add_dependencies(${target} ${target}-conf-dist)
    ambrose_install_executable(${target})
    install(FILES ${dist_files} DESTINATION etc)
endfunction()

function(ambrose_install_executable target)
    if(WIN32)
        install(TARGETS ${target} RUNTIME_DEPENDENCY_SET ambrose_runtime_dependencies RUNTIME DESTINATION bin)
        set_property(GLOBAL PROPERTY AMBROSE_RUNTIME_DEPENDENCIES ON)
    else()
        install(TARGETS ${target} RUNTIME DESTINATION bin)
    endif()
endfunction()

function(ambrose_install_runtime_dependencies)
    get_property(used GLOBAL PROPERTY AMBROSE_RUNTIME_DEPENDENCIES)
    if(NOT used)
        return()
    endif()
    set(vcpkg_root "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    get_filename_component(compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_program(ambrose_dumpbin dumpbin HINTS "${compiler_bin}" NO_DEFAULT_PATH NO_CACHE)
    if(NOT ambrose_dumpbin)
        message(FATAL_ERROR "dumpbin was not found beside ${CMAKE_CXX_COMPILER}, so installs cannot find the DLLs executables load")
    endif()
    install(CODE "set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM \"windows+pe\")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL \"dumpbin\")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND \"${ambrose_dumpbin}\")")
    install(RUNTIME_DEPENDENCY_SET ambrose_runtime_dependencies
        DIRECTORIES "$<IF:$<CONFIG:Debug>,${vcpkg_root}/debug/bin,${vcpkg_root}/bin>"
        PRE_EXCLUDE_REGEXES "^[Aa][Pp][Ii]-[Mm][Ss]-" "^[Ee][Xx][Tt]-[Mm][Ss]-"
        POST_EXCLUDE_REGEXES ".*[Ss][Yy][Ss][Tt][Ee][Mm]32.*"
        DESTINATION bin)
endfunction()

function(ambrose_copy_mariadb_plugins target)
    set(release_plugins "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/plugins/libmariadb")
    set(debug_plugins "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/plugins/libmariadb")
    if(NOT EXISTS "${release_plugins}")
        return()
    endif()
    if(NOT EXISTS "${debug_plugins}")
        set(debug_plugins "${release_plugins}")
    endif()
    if(NOT TARGET ambrose_mariadb_plugins)
        get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
        if(multi_config)
            set(plugin_destination "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/plugins/libmariadb")
        else()
            set(plugin_destination "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/libmariadb")
        endif()
        add_custom_target(ambrose_mariadb_plugins
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "$<IF:$<CONFIG:Debug>,${debug_plugins},${release_plugins}>" "${plugin_destination}"
            VERBATIM)
        install(DIRECTORY "${debug_plugins}/" DESTINATION bin/plugins/libmariadb CONFIGURATIONS Debug)
        install(DIRECTORY "${release_plugins}/" DESTINATION bin/plugins/libmariadb CONFIGURATIONS Release RelWithDebInfo MinSizeRel)
    endif()
    add_dependencies(${target} ambrose_mariadb_plugins)
endfunction()
