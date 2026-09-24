# Project Ambrose by Imjustchico
# Builds schemaprobe, the client-data hash oracle and BINd schema report tool.
ambrose_add_executable(schemaprobe "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(schemaprobe PRIVATE shared)
ambrose_install_executable(schemaprobe)
