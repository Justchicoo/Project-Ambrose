# Project Ambrose by Imjustchico
# Builds the shared library of code every server uses: network, messages, ObjectProperty, archives, locale text, character name tables, client data discovery and extraction, realms, the folder the project's shipped SQL is found in, the sandboxed LuaJIT state the install's Lua scripts run in, and the admin API listener Crow serves, with the certificate OpenSSL serves it over TLS with.
if(NOT TARGET ambrose-luajit)
    foreach(stale IN ITEMS AMBROSE_LUAJIT_INCLUDE_DIR AMBROSE_LUAJIT_LIBRARY_RELEASE AMBROSE_LUAJIT_LIBRARY_DEBUG AMBROSE_LUAJIT_DLL_RELEASE AMBROSE_LUAJIT_DLL_DEBUG)
        unset(${stale} CACHE)
    endforeach()
    find_path(AMBROSE_LUAJIT_INCLUDE_DIR luajit.h PATH_SUFFIXES luajit luajit-2.1 NO_CACHE REQUIRED)
    get_filename_component(luajit_prefix "${AMBROSE_LUAJIT_INCLUDE_DIR}" DIRECTORY)
    get_filename_component(luajit_prefix "${luajit_prefix}" DIRECTORY)
    find_library(AMBROSE_LUAJIT_LIBRARY_RELEASE NAMES luajit-5.1 lua51 PATHS "${luajit_prefix}/lib" NO_DEFAULT_PATH NO_CACHE REQUIRED)
    find_library(AMBROSE_LUAJIT_LIBRARY_DEBUG NAMES luajit-5.1 lua51 PATHS "${luajit_prefix}/debug/lib" NO_DEFAULT_PATH NO_CACHE)
    find_file(AMBROSE_LUAJIT_DLL_RELEASE NAMES lua51.dll PATHS "${luajit_prefix}/bin" NO_DEFAULT_PATH NO_CACHE)
    find_file(AMBROSE_LUAJIT_DLL_DEBUG NAMES lua51.dll PATHS "${luajit_prefix}/debug/bin" NO_DEFAULT_PATH NO_CACHE)
    if(WIN32 AND AMBROSE_LUAJIT_DLL_RELEASE)
        add_library(ambrose-luajit SHARED IMPORTED GLOBAL)
        set_target_properties(ambrose-luajit PROPERTIES
            IMPORTED_LOCATION "${AMBROSE_LUAJIT_DLL_RELEASE}"
            IMPORTED_IMPLIB "${AMBROSE_LUAJIT_LIBRARY_RELEASE}")
        if(AMBROSE_LUAJIT_DLL_DEBUG AND AMBROSE_LUAJIT_LIBRARY_DEBUG)
            set_target_properties(ambrose-luajit PROPERTIES
                IMPORTED_LOCATION_DEBUG "${AMBROSE_LUAJIT_DLL_DEBUG}"
                IMPORTED_IMPLIB_DEBUG "${AMBROSE_LUAJIT_LIBRARY_DEBUG}"
                IMPORTED_LOCATION_RELEASE "${AMBROSE_LUAJIT_DLL_RELEASE}"
                IMPORTED_IMPLIB_RELEASE "${AMBROSE_LUAJIT_LIBRARY_RELEASE}"
                IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
                MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE
                MAP_IMPORTED_CONFIG_MINSIZEREL RELEASE)
        endif()
    else()
        add_library(ambrose-luajit STATIC IMPORTED GLOBAL)
        set_target_properties(ambrose-luajit PROPERTIES IMPORTED_LOCATION "${AMBROSE_LUAJIT_LIBRARY_RELEASE}")
        if(AMBROSE_LUAJIT_LIBRARY_DEBUG)
            set_target_properties(ambrose-luajit PROPERTIES
                IMPORTED_LOCATION_DEBUG "${AMBROSE_LUAJIT_LIBRARY_DEBUG}"
                IMPORTED_LOCATION_RELEASE "${AMBROSE_LUAJIT_LIBRARY_RELEASE}"
                IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
                MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE
                MAP_IMPORTED_CONFIG_MINSIZEREL RELEASE)
        endif()
        if(UNIX)
            set_property(TARGET ambrose-luajit PROPERTY INTERFACE_LINK_LIBRARIES ${CMAKE_DL_LIBS} m)
        endif()
    endif()
    set_target_properties(ambrose-luajit PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${AMBROSE_LUAJIT_INCLUDE_DIR}")
endif()

ambrose_add_library(shared "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(shared
    PUBLIC
        common
    PRIVATE
        ambrose-luajit
        Crow::Crow
        pugixml::pugixml
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        ftxui::dom
        ftxui::screen
        ftxui::component)

target_compile_definitions(shared PRIVATE CROW_ENABLE_LOGGING CROW_DISABLE_STATIC_DIR CROW_ENABLE_SSL "AMBROSE_SOURCE_DIRECTORY=\"${PROJECT_SOURCE_DIR}\"")

if(WIN32)
    target_link_libraries(shared PRIVATE advapi32)
endif()
