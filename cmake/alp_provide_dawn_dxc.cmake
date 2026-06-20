include_guard(GLOBAL)

# DirectX Shader Compiler runtime, pinned (via ALP_DXC_URL, defined next to
# ALP_DAWN_VERSION) to match the prebuilt Dawn package. Returns "dxcompiler.dll;dxil.dll"
# (a matched pair from one directory) in out_var.
function(alp_provide_dawn_dxc_dlls out_var)
    if (NOT WIN32)
        message(FATAL_ERROR "alp_provide_dawn_dxc_dlls can only be used on Windows")
    endif()

    if (NOT ALP_DXC_URL)
        message(FATAL_ERROR "ALP_DXC_URL is not set (expected next to ALP_DAWN_VERSION)")
    endif()

    # Derive the release tag (cache dir) and archive name from the single URL,
    # e.g. .../download/<tag>/<archive>.
    get_filename_component(ALP_DXC_ARCHIVE "${ALP_DXC_URL}" NAME)
    get_filename_component(ALP_DXC_TAG_DIR "${ALP_DXC_URL}" DIRECTORY)
    get_filename_component(ALP_DXC_TAG "${ALP_DXC_TAG_DIR}" NAME)

    set(ALP_DAWN_DXC_DIR "" CACHE PATH "Directory containing dxcompiler.dll and dxil.dll for Dawn's DirectX backend")

    if (ALP_DAWN_DXC_DIR)
        file(TO_CMAKE_PATH "${ALP_DAWN_DXC_DIR}" ALP_DXC_LOCAL_DIR)
        if (NOT EXISTS "${ALP_DXC_LOCAL_DIR}/dxcompiler.dll" OR NOT EXISTS "${ALP_DXC_LOCAL_DIR}/dxil.dll")
            message(FATAL_ERROR
                "ALP_DAWN_DXC_DIR='${ALP_DXC_LOCAL_DIR}' must contain both dxcompiler.dll and dxil.dll")
        endif()
        set(${out_var} "${ALP_DXC_LOCAL_DIR}/dxcompiler.dll" "${ALP_DXC_LOCAL_DIR}/dxil.dll" PARENT_SCOPE)
        return()
    endif()

    set(ALP_DXC_ROOT "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/dxc/${ALP_DXC_TAG}")
    set(ALP_DXC_BIN "${ALP_DXC_ROOT}/bin/x64")

    if (NOT EXISTS "${ALP_DXC_BIN}/dxcompiler.dll" OR NOT EXISTS "${ALP_DXC_BIN}/dxil.dll")
        set(ALP_DXC_ZIP "${ALP_DXC_ROOT}/${ALP_DXC_ARCHIVE}")
        message(STATUS "Fetching DirectX Shader Compiler ${ALP_DXC_TAG} for Dawn...")
        file(DOWNLOAD "${ALP_DXC_URL}" "${ALP_DXC_ZIP}"
            TLS_VERIFY ON
            STATUS ALP_DXC_DOWNLOAD_STATUS
            SHOW_PROGRESS)
        list(GET ALP_DXC_DOWNLOAD_STATUS 0 ALP_DXC_DOWNLOAD_CODE)
        if (NOT ALP_DXC_DOWNLOAD_CODE EQUAL 0)
            list(GET ALP_DXC_DOWNLOAD_STATUS 1 ALP_DXC_DOWNLOAD_MESSAGE)
            file(REMOVE "${ALP_DXC_ZIP}")
            message(FATAL_ERROR
                "Failed to download DXC from ${ALP_DXC_URL}: ${ALP_DXC_DOWNLOAD_MESSAGE}\n"
                "Set -DALP_DAWN_DXC_DIR=<dir with dxcompiler.dll and dxil.dll> to use a local copy.")
        endif()

        file(ARCHIVE_EXTRACT
            INPUT "${ALP_DXC_ZIP}"
            DESTINATION "${ALP_DXC_ROOT}"
            PATTERNS "bin/x64/dxcompiler.dll" "bin/x64/dxil.dll")
        file(REMOVE "${ALP_DXC_ZIP}")

        if (NOT EXISTS "${ALP_DXC_BIN}/dxcompiler.dll" OR NOT EXISTS "${ALP_DXC_BIN}/dxil.dll")
            message(FATAL_ERROR
                "DXC archive ${ALP_DXC_ARCHIVE} did not contain bin/x64/dxcompiler.dll and bin/x64/dxil.dll")
        endif()
    endif()

    set(${out_var} "${ALP_DXC_BIN}/dxcompiler.dll" "${ALP_DXC_BIN}/dxil.dll" PARENT_SCOPE)
endfunction()
