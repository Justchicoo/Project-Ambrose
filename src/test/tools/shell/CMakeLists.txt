# Project Ambrose by Imjustchico
# Builds shell_smoke, which drives the desktop shell's real web view windows off screen, and registers ShellSmoke, which runs it from a folder the user cannot write, against loopback listeners and with two certificates the supervisor's --panel-self-signed writes, reporting itself skipped where no web view exists and labelled `render`, since it needs a web view that really draws.
if(NOT TARGET launcher-core)
    return()
endif()

add_executable(shell_smoke "${CMAKE_CURRENT_SOURCE_DIR}/smoke/ShellSmoke.cpp")
target_link_libraries(shell_smoke PRIVATE ambrose-compile-options launcher-core desktop-shell nlohmann_json::nlohmann_json)

set(shell_smoke_supervisor "")
if(TARGET supervisor)
    set(shell_smoke_supervisor "$<TARGET_FILE:supervisor>")
endif()
if(WIN32)
    get_filename_component(openssl_install_dir "${OPENSSL_INCLUDE_DIR}" DIRECTORY)
    set(openssl_ssl_dll "${openssl_install_dir}/bin/libssl-3-x64.dll")
    set(openssl_crypto_dll "${openssl_install_dir}/bin/libcrypto-3-x64.dll")
    if(NOT EXISTS "${openssl_ssl_dll}" OR NOT EXISTS "${openssl_crypto_dll}")
        message(FATAL_ERROR "ShellSmoke requires the OpenSSL runtime DLLs beside the vcpkg include directory.")
    endif()
    set(shell_smoke_dlls "$<JOIN:$<TARGET_RUNTIME_DLLS:shell_smoke>,|>|${openssl_ssl_dll}|${openssl_crypto_dll}")
else()
    set(shell_smoke_dlls "")
endif()
add_test(NAME ShellSmoke
    COMMAND "${CMAKE_COMMAND}" "-DSMOKE=$<TARGET_FILE:shell_smoke>" "-DSUPERVISOR=${shell_smoke_supervisor}"
        "-DDLLS=${shell_smoke_dlls}" "-DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/shell-smoke"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ShellSmokeTest.cmake")
set_tests_properties(ShellSmoke PROPERTIES SKIP_REGULAR_EXPRESSION "shell smoke skipped" TIMEOUT 600 RUN_SERIAL TRUE LABELS render)
