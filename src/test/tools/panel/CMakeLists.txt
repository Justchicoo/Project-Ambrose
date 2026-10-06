# Project Ambrose by Imjustchico
# Builds panel_smoke, which drives the panel program's own work against the built supervisor with a real web view window off screen, and registers PanelSmoke, which reports itself skipped where no web view exists or no supervisor was built.
if(NOT TARGET supervisor)
    return()
endif()

add_executable(panel_smoke "${CMAKE_CURRENT_SOURCE_DIR}/smoke/PanelSmoke.cpp")
target_link_libraries(panel_smoke PRIVATE ambrose-compile-options panel-core nlohmann_json::nlohmann_json)
add_dependencies(panel_smoke supervisor)

add_test(NAME PanelSmoke
    COMMAND "${CMAKE_COMMAND}" "-DSMOKE=$<TARGET_FILE:panel_smoke>" "-DSUPERVISOR=$<TARGET_FILE:supervisor>" "-DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/panel-smoke"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/PanelSmokeTest.cmake")
set_tests_properties(PanelSmoke PROPERTIES SKIP_REGULAR_EXPRESSION "panel smoke skipped" TIMEOUT 600 RUN_SERIAL TRUE)
