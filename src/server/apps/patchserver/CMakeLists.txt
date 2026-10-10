# Project Ambrose by Imjustchico
# Builds the patching library of the patch server's sessions, message table, manifest and listener that tests link, and the patchserver executable that answers TCP service 8 with where the manifest is on top of it.
ambrose_collect_sources("${CMAKE_CURRENT_SOURCE_DIR}" patching_sources)
list(FILTER patching_sources EXCLUDE REGEX "/Main\\.cpp$")
add_library(patching STATIC ${patching_sources})
ambrose_collect_include_dirs("${CMAKE_CURRENT_SOURCE_DIR}" patching_include_dirs)
target_include_directories(patching PUBLIC ${patching_include_dirs})
target_link_libraries(patching PUBLIC ambrose-compile-options shared)

add_executable(patchserver "${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp")
target_link_libraries(patchserver PRIVATE ambrose-compile-options patching)
ambrose_copy_conf_dist(patchserver)
