# Project Ambrose by Imjustchico
# Builds typeregbuild, which wraps a user's JSON type dump in the validated binary registry cache format.
add_executable(typeregbuild Main.cpp)
target_link_libraries(typeregbuild PRIVATE ambrose-compile-options shared)
ambrose_install_executable(typeregbuild)
