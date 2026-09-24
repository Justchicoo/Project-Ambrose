# Project Ambrose by Imjustchico
# Builds zone_extractor, the tool that reports and exports WizZoneData from the user's own client archives.
ambrose_add_executable(zone_extractor "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(zone_extractor PRIVATE shared)
ambrose_install_executable(zone_extractor)
