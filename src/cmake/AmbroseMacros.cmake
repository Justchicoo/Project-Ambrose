# Project Ambrose by Imjustchico
# Helpers that build a library or executable from every source file under a folder.
function(ambrose_collect_include_dirs root out)
    set(dirs "${root}")
    file(GLOB_RECURSE children LIST_DIRECTORIES true "${root}/*")
    foreach(child IN LISTS children)
        if(IS_DIRECTORY "${child}")
            list(APPEND dirs "${child}")
        endif()
    endforeach()
    set(${out} "${dirs}" PARENT_SCOPE)
endfunction()

function(ambrose_collect_sources root out)
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS
        "${root}/*.cpp"
        "${root}/*.h")
    set(${out} "${sources}" PARENT_SCOPE)
endfunction()

function(ambrose_add_library name root)
    ambrose_collect_sources("${root}" sources)
    add_library(${name} STATIC ${sources})
    ambrose_collect_include_dirs("${root}" include_dirs)
    target_include_directories(${name} PUBLIC ${include_dirs})
    target_link_libraries(${name} PUBLIC ambrose-compile-options)
endfunction()

function(ambrose_add_executable name root)
    ambrose_collect_sources("${root}" sources)
    add_executable(${name} ${sources})
    target_link_libraries(${name} PRIVATE ambrose-compile-options)
endfunction()
