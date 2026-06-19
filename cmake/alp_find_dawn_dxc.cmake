include_guard(GLOBAL)

function(_alp_dawn_dxc_arch_dirs out_var)
    if (CMAKE_GENERATOR_PLATFORM MATCHES "(^|,)([Aa][Rr][Mm]64|aarch64)($|,)")
        set(ALP_DAWN_DXC_PRIMARY_ARCH arm64)
    elseif(CMAKE_GENERATOR_PLATFORM MATCHES "(^|,)([Ww]in32|x86)($|,)")
        set(ALP_DAWN_DXC_PRIMARY_ARCH x86)
    elseif(CMAKE_GENERATOR_PLATFORM MATCHES "(^|,)(x64|X64|amd64|AMD64)($|,)")
        set(ALP_DAWN_DXC_PRIMARY_ARCH x64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
        set(ALP_DAWN_DXC_PRIMARY_ARCH arm64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(ALP_DAWN_DXC_PRIMARY_ARCH x86)
    else()
        set(ALP_DAWN_DXC_PRIMARY_ARCH x64)
    endif()

    set(ALP_DAWN_DXC_ARCH_DIRS "${ALP_DAWN_DXC_PRIMARY_ARCH}" x64 arm64 x86)
    list(REMOVE_DUPLICATES ALP_DAWN_DXC_ARCH_DIRS)
    set(${out_var} "${ALP_DAWN_DXC_ARCH_DIRS}" PARENT_SCOPE)
endfunction()

function(_alp_append_existing_dir out_var dir)
    if (dir)
        file(TO_CMAKE_PATH "${dir}" ALP_DAWN_DXC_DIR_CMAKE)
        if (EXISTS "${ALP_DAWN_DXC_DIR_CMAKE}")
            set(ALP_DAWN_DXC_DIRS "${${out_var}}" "${ALP_DAWN_DXC_DIR_CMAKE}")
            set(${out_var} "${ALP_DAWN_DXC_DIRS}" PARENT_SCOPE)
        endif()
    endif()
endfunction()

function(_alp_dawn_dxc_windows_sdk_roots out_var)
    set(ALP_DAWN_DXC_SDK_ROOTS)

    _alp_append_existing_dir(ALP_DAWN_DXC_SDK_ROOTS "$ENV{WINDOWSSDKDIR}")
    _alp_append_existing_dir(ALP_DAWN_DXC_SDK_ROOTS "$ENV{WindowsSdkDir}")
    _alp_append_existing_dir(ALP_DAWN_DXC_SDK_ROOTS "$ENV{WIN10_SDK_PATH}")
    _alp_append_existing_dir(ALP_DAWN_DXC_SDK_ROOTS "$ENV{CMAKE_WINDOWS_KITS_10_DIR}")

    cmake_host_system_information(
        RESULT ALP_DAWN_DXC_REGISTRY_SDK_ROOT
        QUERY WINDOWS_REGISTRY "HKLM/SOFTWARE/Microsoft/Windows Kits/Installed Roots"
        VALUE "KitsRoot10"
        VIEW BOTH
        ERROR_VARIABLE ALP_DAWN_DXC_REGISTRY_ERROR
    )
    _alp_append_existing_dir(ALP_DAWN_DXC_SDK_ROOTS "${ALP_DAWN_DXC_REGISTRY_SDK_ROOT}")

    set(ALP_DAWN_DXC_PROGRAM_FILES_X86_ENV "ProgramFiles(x86)")
    if (DEFINED ENV{${ALP_DAWN_DXC_PROGRAM_FILES_X86_ENV}})
        _alp_append_existing_dir(ALP_DAWN_DXC_SDK_ROOTS "$ENV{${ALP_DAWN_DXC_PROGRAM_FILES_X86_ENV}}/Windows Kits/10")
    endif()
    _alp_append_existing_dir(ALP_DAWN_DXC_SDK_ROOTS "$ENV{ProgramFiles}/Windows Kits/10")

    if (ALP_DAWN_DXC_SDK_ROOTS)
        list(REMOVE_DUPLICATES ALP_DAWN_DXC_SDK_ROOTS)
    endif()
    set(${out_var} "${ALP_DAWN_DXC_SDK_ROOTS}" PARENT_SCOPE)
endfunction()

function(_alp_dawn_dxc_sdk_versions out_var sdk_root)
    set(ALP_DAWN_DXC_SDK_VERSIONS)

    if (CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        list(APPEND ALP_DAWN_DXC_SDK_VERSIONS "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
    endif()

    foreach(ALP_DAWN_DXC_VERSION_ENV IN ITEMS WindowsSDKVersion WIN10_SDK_VERSION)
        if (DEFINED ENV{${ALP_DAWN_DXC_VERSION_ENV}} AND NOT "$ENV{${ALP_DAWN_DXC_VERSION_ENV}}" STREQUAL "")
            set(ALP_DAWN_DXC_ENV_SDK_VERSION "$ENV{${ALP_DAWN_DXC_VERSION_ENV}}")
            string(REGEX REPLACE "[/\\\\]+$" "" ALP_DAWN_DXC_ENV_SDK_VERSION "${ALP_DAWN_DXC_ENV_SDK_VERSION}")
            list(APPEND ALP_DAWN_DXC_SDK_VERSIONS "${ALP_DAWN_DXC_ENV_SDK_VERSION}")
        endif()
    endforeach()

    foreach(ALP_DAWN_DXC_VERSION IN LISTS ALP_DAWN_DXC_SDK_VERSIONS)
        if (IS_DIRECTORY "${sdk_root}/Include/${ALP_DAWN_DXC_VERSION}.0")
            list(APPEND ALP_DAWN_DXC_SDK_VERSIONS "${ALP_DAWN_DXC_VERSION}.0")
        endif()
    endforeach()

    file(GLOB ALP_DAWN_DXC_INSTALLED_SDK_DIRS LIST_DIRECTORIES true "${sdk_root}/Include/10.*")
    if (ALP_DAWN_DXC_INSTALLED_SDK_DIRS)
        list(SORT ALP_DAWN_DXC_INSTALLED_SDK_DIRS COMPARE NATURAL ORDER DESCENDING)
        foreach(ALP_DAWN_DXC_INSTALLED_SDK_DIR IN LISTS ALP_DAWN_DXC_INSTALLED_SDK_DIRS)
            get_filename_component(ALP_DAWN_DXC_INSTALLED_SDK_VERSION "${ALP_DAWN_DXC_INSTALLED_SDK_DIR}" NAME)
            list(APPEND ALP_DAWN_DXC_SDK_VERSIONS "${ALP_DAWN_DXC_INSTALLED_SDK_VERSION}")
        endforeach()
    endif()

    if (ALP_DAWN_DXC_SDK_VERSIONS)
        list(REMOVE_DUPLICATES ALP_DAWN_DXC_SDK_VERSIONS)
    endif()
    set(${out_var} "${ALP_DAWN_DXC_SDK_VERSIONS}" PARENT_SCOPE)
endfunction()

function(alp_find_dawn_dxc_dlls out_var)
    if (NOT WIN32)
        message(FATAL_ERROR "alp_find_dawn_dxc_dlls can only be used on Windows")
    endif()

    set(ALP_DAWN_DXC_DIR "" CACHE PATH "Directory containing dxcompiler.dll and dxil.dll for Dawn's DirectX backend")
    set(ALP_DAWN_DXC_HINT_DIRS)
    set(ALP_DAWN_DXC_SEARCH_LABELS)

    if (ALP_DAWN_DXC_DIR)
        file(TO_CMAKE_PATH "${ALP_DAWN_DXC_DIR}" ALP_DAWN_DXC_EXPLICIT_DIR)
        list(APPEND ALP_DAWN_DXC_HINT_DIRS "${ALP_DAWN_DXC_EXPLICIT_DIR}")
        list(APPEND ALP_DAWN_DXC_SEARCH_LABELS "${ALP_DAWN_DXC_EXPLICIT_DIR}")
    else()
        _alp_dawn_dxc_arch_dirs(ALP_DAWN_DXC_ARCH_DIRS)
        _alp_dawn_dxc_windows_sdk_roots(ALP_DAWN_DXC_SDK_ROOTS)

        foreach(ALP_DAWN_DXC_SDK_ROOT IN LISTS ALP_DAWN_DXC_SDK_ROOTS)
            _alp_dawn_dxc_sdk_versions(ALP_DAWN_DXC_SDK_VERSIONS "${ALP_DAWN_DXC_SDK_ROOT}")

            foreach(ALP_DAWN_DXC_ARCH_DIR IN LISTS ALP_DAWN_DXC_ARCH_DIRS)
                list(APPEND ALP_DAWN_DXC_HINT_DIRS "${ALP_DAWN_DXC_SDK_ROOT}/Redist/D3D/${ALP_DAWN_DXC_ARCH_DIR}")
            endforeach()

            foreach(ALP_DAWN_DXC_SDK_VERSION IN LISTS ALP_DAWN_DXC_SDK_VERSIONS)
                foreach(ALP_DAWN_DXC_ARCH_DIR IN LISTS ALP_DAWN_DXC_ARCH_DIRS)
                    list(APPEND ALP_DAWN_DXC_HINT_DIRS "${ALP_DAWN_DXC_SDK_ROOT}/bin/${ALP_DAWN_DXC_SDK_VERSION}/${ALP_DAWN_DXC_ARCH_DIR}")
                endforeach()
            endforeach()

            foreach(ALP_DAWN_DXC_ARCH_DIR IN LISTS ALP_DAWN_DXC_ARCH_DIRS)
                list(APPEND ALP_DAWN_DXC_HINT_DIRS "${ALP_DAWN_DXC_SDK_ROOT}/bin/${ALP_DAWN_DXC_ARCH_DIR}")
            endforeach()
        endforeach()
    endif()

    if (ALP_DAWN_DXC_HINT_DIRS)
        list(REMOVE_DUPLICATES ALP_DAWN_DXC_HINT_DIRS)
        foreach(ALP_DAWN_DXC_HINT_DIR IN LISTS ALP_DAWN_DXC_HINT_DIRS)
            if (EXISTS "${ALP_DAWN_DXC_HINT_DIR}")
                list(APPEND ALP_DAWN_DXC_SEARCH_LABELS "${ALP_DAWN_DXC_HINT_DIR}")
            endif()
        endforeach()
    endif()

    find_file(_ALP_DAWN_DXCOMPILER_DLL
        NAMES dxcompiler.dll
        HINTS ${ALP_DAWN_DXC_HINT_DIRS}
        NO_DEFAULT_PATH
        NO_CACHE
    )
    find_file(_ALP_DAWN_DXIL_DLL
        NAMES dxil.dll
        HINTS ${ALP_DAWN_DXC_HINT_DIRS}
        NO_DEFAULT_PATH
        NO_CACHE
    )

    if (NOT ALP_DAWN_DXC_DIR)
        if (NOT _ALP_DAWN_DXCOMPILER_DLL)
            find_file(_ALP_DAWN_DXCOMPILER_DLL NAMES dxcompiler.dll NO_CACHE)
        endif()
        if (NOT _ALP_DAWN_DXIL_DLL)
            find_file(_ALP_DAWN_DXIL_DLL NAMES dxil.dll NO_CACHE)
        endif()
        list(APPEND ALP_DAWN_DXC_SEARCH_LABELS "CMake default search paths")
    endif()

    if (NOT _ALP_DAWN_DXCOMPILER_DLL OR NOT _ALP_DAWN_DXIL_DLL)
        if (ALP_DAWN_DXC_SEARCH_LABELS)
            list(REMOVE_DUPLICATES ALP_DAWN_DXC_SEARCH_LABELS)
            string(REPLACE ";" "\n  " ALP_DAWN_DXC_SEARCH_PATHS "${ALP_DAWN_DXC_SEARCH_LABELS}")
        else()
            set(ALP_DAWN_DXC_SEARCH_PATHS "No candidate paths were found")
        endif()

        message(FATAL_ERROR
            "Dawn's Windows package does not ship the DirectX Shader Compiler runtime DLLs, "
            "and CMake could not find dxcompiler.dll and dxil.dll.\n"
            "Set -DALP_DAWN_DXC_DIR=<directory containing dxcompiler.dll and dxil.dll>.\n"
            "Searched:\n  ${ALP_DAWN_DXC_SEARCH_PATHS}"
        )
    endif()

    set(${out_var} "${_ALP_DAWN_DXCOMPILER_DLL}" "${_ALP_DAWN_DXIL_DLL}" PARENT_SCOPE)
endfunction()
