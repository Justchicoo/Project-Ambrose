# Project Ambrose by Imjustchico
# Compiles through ccache when AMBROSE_COMPILER_CACHE is on, ccache is installed and the generator runs a compiler launcher, so a tree rebuilt after a branch switch or a rebase reuses every object whose inputs did not change, with the clone's own folder as ccache's base and the working folder left out of the hash, so clones in other folders share the same objects; ccache then hands the compiler paths relative to the build folder, so under GCC and Clang the source folder seen from there is mapped away too and __FILE__ stays relative to the repository, while MSVC under Ninja, which takes no prefix map, caches its objects with the debug information embedded in each one.
if(AMBROSE_COMPILER_CACHE AND NOT CMAKE_CXX_COMPILER_LAUNCHER AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    find_program(AMBROSE_CCACHE ccache)
    if(AMBROSE_CCACHE)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CMAKE_COMMAND};-E;env;CCACHE_BASEDIR=${CMAKE_SOURCE_DIR};CCACHE_NOHASHDIR=1;${AMBROSE_CCACHE}")
        if(NOT MSVC)
            file(RELATIVE_PATH AMBROSE_SOURCE_FROM_BUILD "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}")
            string(REGEX REPLACE "/+$" "" AMBROSE_SOURCE_FROM_BUILD "${AMBROSE_SOURCE_FROM_BUILD}")
            add_compile_options("-fmacro-prefix-map=${AMBROSE_SOURCE_FROM_BUILD}/=")
        endif()
        message(STATUS "Compiling through ${AMBROSE_CCACHE}")
    endif()
endif()
