# Project Ambrose by Imjustchico
# Builds typeextract, the tool that emulates the user's own client program to build its type dump: the typeextract-core library links Unicorn (GPL-2.0, a DLL on Windows and a shared library on Linux) and Zydis, and the executable is a front end over it.
find_package(zycore CONFIG REQUIRED)
find_package(zydis CONFIG REQUIRED)

if(NOT TARGET ambrose-unicorn)
    foreach(stale IN ITEMS AMBROSE_UNICORN_INCLUDE_DIR AMBROSE_UNICORN_LIBRARY_RELEASE AMBROSE_UNICORN_LIBRARY_DEBUG AMBROSE_UNICORN_DLL_RELEASE AMBROSE_UNICORN_DLL_DEBUG)
        unset(${stale} CACHE)
    endforeach()
    find_path(AMBROSE_UNICORN_INCLUDE_DIR unicorn/unicorn.h NO_CACHE REQUIRED)
    get_filename_component(unicorn_prefix "${AMBROSE_UNICORN_INCLUDE_DIR}" DIRECTORY)
    find_library(AMBROSE_UNICORN_LIBRARY_RELEASE NAMES unicorn PATHS "${unicorn_prefix}/lib" NO_DEFAULT_PATH NO_CACHE REQUIRED)
    find_library(AMBROSE_UNICORN_LIBRARY_DEBUG NAMES unicorn PATHS "${unicorn_prefix}/debug/lib" NO_DEFAULT_PATH NO_CACHE)
    find_file(AMBROSE_UNICORN_DLL_RELEASE NAMES unicorn.dll PATHS "${unicorn_prefix}/bin" NO_DEFAULT_PATH NO_CACHE)
    find_file(AMBROSE_UNICORN_DLL_DEBUG NAMES unicorn.dll PATHS "${unicorn_prefix}/debug/bin" NO_DEFAULT_PATH NO_CACHE)
    if(WIN32 AND AMBROSE_UNICORN_DLL_RELEASE)
        add_library(ambrose-unicorn SHARED IMPORTED GLOBAL)
        set_target_properties(ambrose-unicorn PROPERTIES
            IMPORTED_LOCATION "${AMBROSE_UNICORN_DLL_RELEASE}"
            IMPORTED_IMPLIB "${AMBROSE_UNICORN_LIBRARY_RELEASE}"
            IMPORTED_LOCATION_RELEASE "${AMBROSE_UNICORN_DLL_RELEASE}"
            IMPORTED_IMPLIB_RELEASE "${AMBROSE_UNICORN_LIBRARY_RELEASE}")
        if(AMBROSE_UNICORN_DLL_DEBUG AND AMBROSE_UNICORN_LIBRARY_DEBUG)
            set_target_properties(ambrose-unicorn PROPERTIES
                IMPORTED_LOCATION_DEBUG "${AMBROSE_UNICORN_DLL_DEBUG}"
                IMPORTED_IMPLIB_DEBUG "${AMBROSE_UNICORN_LIBRARY_DEBUG}"
                IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
                MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE
                MAP_IMPORTED_CONFIG_MINSIZEREL RELEASE)
        endif()
    elseif(AMBROSE_UNICORN_LIBRARY_RELEASE MATCHES "\.so$")
        add_library(ambrose-unicorn SHARED IMPORTED GLOBAL)
        set_target_properties(ambrose-unicorn PROPERTIES
            IMPORTED_LOCATION "${AMBROSE_UNICORN_LIBRARY_RELEASE}"
            IMPORTED_LOCATION_RELEASE "${AMBROSE_UNICORN_LIBRARY_RELEASE}"
            IMPORTED_NO_SONAME FALSE)
        if(AMBROSE_UNICORN_LIBRARY_DEBUG)
            set_target_properties(ambrose-unicorn PROPERTIES
                IMPORTED_LOCATION_DEBUG "${AMBROSE_UNICORN_LIBRARY_DEBUG}"
                IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
                MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE
                MAP_IMPORTED_CONFIG_MINSIZEREL RELEASE)
        endif()
        set_property(TARGET ambrose-unicorn APPEND PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads m)
    else()
        add_library(ambrose-unicorn UNKNOWN IMPORTED GLOBAL)
        set_target_properties(ambrose-unicorn PROPERTIES
            IMPORTED_LOCATION "${AMBROSE_UNICORN_LIBRARY_RELEASE}"
            IMPORTED_LOCATION_RELEASE "${AMBROSE_UNICORN_LIBRARY_RELEASE}")
        if(AMBROSE_UNICORN_LIBRARY_DEBUG)
            set_target_properties(ambrose-unicorn PROPERTIES
                IMPORTED_LOCATION_DEBUG "${AMBROSE_UNICORN_LIBRARY_DEBUG}"
                IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
                MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE
                MAP_IMPORTED_CONFIG_MINSIZEREL RELEASE)
        endif()
        if(NOT WIN32)
            set_property(TARGET ambrose-unicorn APPEND PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads m)
        endif()
    endif()
    set_target_properties(ambrose-unicorn PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${AMBROSE_UNICORN_INCLUDE_DIR}")
endif()

ambrose_add_library(typeextract-core "${CMAKE_CURRENT_SOURCE_DIR}/Core")
target_link_libraries(typeextract-core
    PUBLIC
        shared
        ambrose-unicorn
        Zydis::Zydis
    PRIVATE
        pugixml::pugixml
        nlohmann_json::nlohmann_json)

add_executable(typeextract "${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp")
target_link_libraries(typeextract PRIVATE ambrose-compile-options typeextract-core)
ambrose_install_executable(typeextract)
if(NOT WIN32 AND AMBROSE_UNICORN_LIBRARY_RELEASE MATCHES "\.so$")
    set_target_properties(typeextract PROPERTIES INSTALL_RPATH "$ORIGIN")
    file(GLOB unicorn_shared_files "${AMBROSE_UNICORN_LIBRARY_RELEASE}*")
    install(FILES ${unicorn_shared_files} DESTINATION bin)
endif()
