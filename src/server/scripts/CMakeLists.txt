# Project Ambrose by Imjustchico
# Builds the scripts library from every script in this folder, every module in modules/, and the loader CMake writes from the AddSC functions it finds in both, so a script or a module joins the build by being there.
ambrose_write_script_loader("${CMAKE_CURRENT_BINARY_DIR}/ScriptLoader.cpp")

ambrose_collect_sources("${CMAKE_CURRENT_SOURCE_DIR}" script_sources)
ambrose_collect_module_sources(module_sources)

add_library(scripts STATIC ${script_sources} ${module_sources} "${CMAKE_CURRENT_BINARY_DIR}/ScriptLoader.cpp")

ambrose_collect_include_dirs("${CMAKE_CURRENT_SOURCE_DIR}" script_include_dirs)
ambrose_collect_module_include_dirs(module_include_dirs)
target_include_directories(scripts PUBLIC ${script_include_dirs} ${module_include_dirs})

target_link_libraries(scripts
    PUBLIC
        ambrose-compile-options
        scripting
        chat
        world
        globals)
