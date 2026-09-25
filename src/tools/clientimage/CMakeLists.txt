# Project Ambrose by Imjustchico
# Builds client-image, the library that reads the user's own client program without running it: its PE sections, exports, imports, relocations, TLS and the functions its exception table lists, and an index of its code built with Zydis. typeextract emulates the program over it and the client tool asks it where the program registers its message handlers, and it links no emulator, so the client tool stays MIT.
find_package(zycore CONFIG REQUIRED)
find_package(zydis CONFIG REQUIRED)

ambrose_add_library(client-image "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(client-image
    PUBLIC
        shared
        Zydis::Zydis)
