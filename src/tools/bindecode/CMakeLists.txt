# Project Ambrose by Imjustchico
# Builds bindecode, the tool that prints BINd entries of the user's own client archives as JSON and sweeps them for decode problems.
ambrose_add_executable(bindecode "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(bindecode PRIVATE shared)
install(TARGETS bindecode RUNTIME DESTINATION bin)
