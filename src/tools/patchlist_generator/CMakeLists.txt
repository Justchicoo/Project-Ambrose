# Project Ambrose by Imjustchico
# Builds patchlist_generator, the install scanner that emits a patch manifest from the user's own files.
ambrose_add_executable(patchlist_generator "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(patchlist_generator PRIVATE shared)
ambrose_copy_conf_dist(patchlist_generator)
