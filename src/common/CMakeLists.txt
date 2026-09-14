# Project Ambrose by Imjustchico
# Builds the common library of game-agnostic foundations.
ambrose_add_library(common "${CMAKE_CURRENT_SOURCE_DIR}")

if(TARGET botan::botan)
    set(ambrose_botan_target botan::botan)
else()
    set(ambrose_botan_target botan::botan-static)
endif()

target_compile_definitions(common
    PUBLIC
        ASIO_STANDALONE
        ASIO_NO_DEPRECATED)

target_link_libraries(common
    PUBLIC
        fmt::fmt
        asio::asio
        Threads::Threads
    PRIVATE
        genrev
        ${ambrose_botan_target})
