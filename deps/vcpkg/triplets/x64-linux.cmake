# Project Ambrose by Imjustchico
# The stock x64-linux triplet, except that Unicorn builds as a shared library, because its static library defines crc32 as zlib's does.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

if(PORT STREQUAL "unicorn")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()
