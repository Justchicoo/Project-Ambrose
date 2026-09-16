# Project Ambrose by Imjustchico
# Builds localetool, the tool that finds, checks and dumps locale keys from the .lang files of the user's own install.
ambrose_add_executable(localetool "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(localetool PRIVATE shared)
install(TARGETS localetool RUNTIME DESTINATION bin)
