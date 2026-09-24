# Project Ambrose by Imjustchico
# Builds the patchserver executable that serves client revision files.
ambrose_add_executable(patchserver "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(patchserver PRIVATE shared)
ambrose_copy_conf_dist(patchserver)
