# Project Ambrose by Imjustchico
# Builds schemaprobe, the client-data hash oracle and BINd schema report tool, which reads the client program's own strings through client-image to name classes.
ambrose_add_executable(schemaprobe "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(schemaprobe PRIVATE shared client-image)
ambrose_install_executable(schemaprobe)
