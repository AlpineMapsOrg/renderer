include_guard(GLOBAL)

# Adds KTX-Software (via alp_add_git_repository, pinned to commitish) and applies the
# feature-flag, Emscripten, and multi-config output-directory tweaks the ktx target needs.
function(alp_setup_ktx commitish)
    set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_JS OFF CACHE BOOL "" FORCE)

    # KTX installs its libs into a versioned subdirectory by default; override so they
    # land next to everything else, then restore the previous value for other targets.
    set(CMAKE_INSTALL_BINDIR_SAVED ${CMAKE_INSTALL_BINDIR})
    set(CMAKE_INSTALL_BINDIR "." CACHE STRING "" FORCE)
    alp_add_git_repository(libktx URL https://github.com/KhronosGroup/KTX-Software.git COMMITISH ${commitish})
    set(CMAKE_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR_SAVED} CACHE STRING "" FORCE)

    if (EMSCRIPTEN AND ALP_ENABLE_THREADING)
        target_compile_options(ktx PUBLIC -pthread)
        target_link_options(ktx PUBLIC -pthread)
    endif()

    if (EMSCRIPTEN AND TARGET ktx)
        get_target_property(KTX_INTERFACE_LINK_OPTIONS ktx INTERFACE_LINK_OPTIONS)
        if (KTX_INTERFACE_LINK_OPTIONS)
            list(FILTER KTX_INTERFACE_LINK_OPTIONS EXCLUDE REGEX "STACK_SIZE=96kb")
            set_target_properties(ktx PROPERTIES INTERFACE_LINK_OPTIONS "${KTX_INTERFACE_LINK_OPTIONS}")
        endif()
    endif()

    if (NOT EMSCRIPTEN)
        # NOTE: KTX builds into an additional Release/Debug directory. The following
        # moves the builds directory for the targets ktx and ktx_read to the binary dir
        foreach(ktx_target ktx ktx_read)
            if (TARGET ${ktx_target})
                set_target_properties(${ktx_target} PROPERTIES
                    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
                    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
                    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
                )
            endif()
        endforeach()
    endif()
endfunction()
