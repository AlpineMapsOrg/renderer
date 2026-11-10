include_guard(GLOBAL)

if(NOT COMMAND alp_setup_cmake_project)
    include(${CMAKE_CURRENT_LIST_DIR}/SetupCMakeProject.cmake)
endif()

function(_alp_append_ktx_interface_property target property)
    get_target_property(_values ${target} ${property})
    if(NOT _values)
        set(_values)
    endif()
    list(APPEND _values ${ARGN})
    set_target_properties(${target} PROPERTIES ${property} "${_values}")
endfunction()

# Builds and installs KTX-Software in an isolated CMake invocation, then imports
# the installed package. This keeps KTX's global CMake state out of the renderer.
function(alp_setup_ktx commitish)
    set(_ktx_build_shared ON)
    if(EMSCRIPTEN OR ANDROID)
        set(_ktx_build_shared OFF)
    endif()

    set(_ktx_cmake_args
        -DKTX_FEATURE_TESTS:BOOL=OFF
        -DKTX_FEATURE_TOOLS:BOOL=OFF
        -DKTX_FEATURE_DOC:BOOL=OFF
        -DKTX_FEATURE_JS:BOOL=OFF
        -DKTX_FEATURE_LOADTEST_APPS:STRING=OFF
        "-DBUILD_SHARED_LIBS:BOOL=${_ktx_build_shared}"
        -DCMAKE_INSTALL_BINDIR:STRING=.
    )

    if (EMSCRIPTEN AND ALP_ENABLE_THREADING)
        set(_ktx_c_flags "${CMAKE_C_FLAGS} -pthread")
        set(_ktx_cxx_flags "${CMAKE_CXX_FLAGS} -pthread")
        set(_ktx_exe_linker_flags "${CMAKE_EXE_LINKER_FLAGS} -pthread")
        set(_ktx_shared_linker_flags "${CMAKE_SHARED_LINKER_FLAGS} -pthread")
        list(APPEND _ktx_cmake_args
            "-DCMAKE_C_FLAGS:STRING=${_ktx_c_flags}"
            "-DCMAKE_CXX_FLAGS:STRING=${_ktx_cxx_flags}"
            "-DCMAKE_EXE_LINKER_FLAGS:STRING=${_ktx_exe_linker_flags}"
            "-DCMAKE_SHARED_LINKER_FLAGS:STRING=${_ktx_shared_linker_flags}"
        )
    endif()

    alp_setup_cmake_project(libktx
        URL https://github.com/KhronosGroup/KTX-Software.git
        COMMITISH ${commitish}
        CMAKE_ARGUMENTS ${_ktx_cmake_args}
    )

    find_package(Ktx CONFIG REQUIRED
        PATHS "${ALP_libktx_INSTALL_DIR}"
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH)
    # Global, so targets in other directories (the macOS bundle step of webgpu_app) can name it.
    set_target_properties(KTX::ktx PROPERTIES IMPORTED_GLOBAL TRUE)

    if (EMSCRIPTEN AND ALP_ENABLE_THREADING)
        _alp_append_ktx_interface_property(KTX::ktx INTERFACE_COMPILE_OPTIONS -pthread)
        _alp_append_ktx_interface_property(KTX::ktx INTERFACE_LINK_OPTIONS -pthread)
    endif()

    if (EMSCRIPTEN AND TARGET KTX::ktx)
        get_target_property(KTX_INTERFACE_LINK_OPTIONS KTX::ktx INTERFACE_LINK_OPTIONS)
        if (KTX_INTERFACE_LINK_OPTIONS)
            list(FILTER KTX_INTERFACE_LINK_OPTIONS EXCLUDE REGEX "STACK_SIZE=96kb")
            set_target_properties(KTX::ktx PROPERTIES INTERFACE_LINK_OPTIONS "${KTX_INTERFACE_LINK_OPTIONS}")
        endif()
    endif()
endfunction()
