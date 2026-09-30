# Project Ambrose by Imjustchico
# Compiles a built web page into a target: ambrose_embed_page generates one translation unit and header, never committed, from a Vite dist folder at build time, rewriting them only when the folder's files change, and a folder with no index.html embeds a page saying what the build left out, so a program built without the front end still opens a page.
set(AMBROSE_EMBED_PAGE_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/EmbedPageGenerate.cmake")

function(ambrose_embed_page target)
    cmake_parse_arguments(PARSE_ARGV 1 embed "" "NAME;FOLDER;PLACEHOLDER" "DEPENDS")
    set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/embedded")
    set(source "${out_dir}/${embed_NAME}.cpp")
    set(header "${out_dir}/${embed_NAME}.h")
    add_custom_target(${target}-${embed_NAME}
        COMMAND "${CMAKE_COMMAND}" "-DFOLDER=${embed_FOLDER}" "-DNAME=${embed_NAME}" "-DSOURCE=${source}" "-DHEADER=${header}"
            "-DPLACEHOLDER=${embed_PLACEHOLDER}" -P "${AMBROSE_EMBED_PAGE_SCRIPT}"
        BYPRODUCTS "${source}" "${header}"
        COMMENT "Embedding ${embed_FOLDER} as ${embed_NAME}"
        VERBATIM)
    if(embed_DEPENDS)
        add_dependencies(${target}-${embed_NAME} ${embed_DEPENDS})
    endif()
    target_sources(${target} PRIVATE "${source}" "${header}")
    target_include_directories(${target} PUBLIC "${out_dir}")
    add_dependencies(${target} ${target}-${embed_NAME})
endfunction()
