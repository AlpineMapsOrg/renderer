#############################################################################
# weBIGeo
# Copyright (C) 2024 Gerald Kimmersdorfer
# Copyright (C) 2025 Patrick Komon
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#############################################################################

project(alpine-renderer-webgpu LANGUAGES C CXX)

# NOTE: The idea behind this target is to accumulate all the webgpu related things that are platform
# dependent such that we only have the webgpu cmake target for the other projects to link against.

set(ALP_DAWN_VERSION "20260420.230544") # Version of Dawn to download (see https://github.com/google/dawn/releases)
set(ALP_DXC_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/dxc_2026_02_20.zip"
    CACHE INTERNAL "DXC release archive whose dxcompiler.dll/dxil.dll match ALP_DAWN_VERSION")
set(ALP_DAWN_DIR "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/dawn")
set(ALP_SDL_INSTALL_DIR "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/sdl")
set(ALP_DAWN_PORT_DIR "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/emdawnwebgpu_pkg/emdawnwebgpu.port.py")

set(SOURCES

    raii/base_types.h
    raii/Texture.h raii/Texture.cpp
    raii/TextureWithSampler.h raii/TextureWithSampler.cpp
    raii/RawBuffer.h
    Buffer.h
    raii/TextureView.h raii/TextureView.cpp
    raii/Sampler.h raii/Sampler.cpp
    raii/BindGroup.h raii/BindGroup.cpp
    raii/BindGroupLayout.h raii/BindGroupLayout.cpp
    raii/Pipeline.h raii/Pipeline.cpp
    raii/PipelineLayout.h raii/PipelineLayout.cpp
    raii/CombinedComputePipeline.h raii/CombinedComputePipeline.cpp
    raii/RenderPassEncoder.h raii/RenderPassEncoder.cpp

    util/VertexBufferInfo.h util/VertexBufferInfo.cpp
    util/VertexFormat.h
    util/string_cast.h util/string_cast.cpp
    util/ShaderPreprocessor.h util/ShaderPreprocessor.cpp

    timing/GuiTimerManager.h timing/GuiTimerManager.cpp
    timing/TimerInterface.h timing/TimerInterface.cpp
    timing/CpuTimer.h timing/CpuTimer.cpp
    timing/WebGpuTimer.h timing/WebGpuTimer.cpp

    Framebuffer.h Framebuffer.cpp

    Context.h
    RenderResourceRegistry.h RenderResourceRegistry.cpp
    gpu_utils.h gpu_utils.cpp
    wgpu_string.h
    webgpu_interface.hpp webgpu_interface.cpp)

add_library(webgpu STATIC ${SOURCES})

find_package(Python3 COMPONENTS Interpreter REQUIRED)

function(alp_find_local_dawn_package out_var)
    set(ALP_DAWN_INSTALL_DIR "${ALP_DAWN_DIR}/install/${ALP_DAWN_CONFIG}")
    set(ALP_DAWN_VERSION_FILE "${ALP_DAWN_INSTALL_DIR}/.alp_dawn_version")
    if (NOT EXISTS "${ALP_DAWN_VERSION_FILE}")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    file(READ "${ALP_DAWN_VERSION_FILE}" ALP_DAWN_INSTALLED_VERSION)
    string(STRIP "${ALP_DAWN_INSTALLED_VERSION}" ALP_DAWN_INSTALLED_VERSION)
    if (NOT ALP_DAWN_INSTALLED_VERSION STREQUAL ALP_DAWN_VERSION)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    foreach(ALP_DAWN_LIB_DIR lib lib64)
        set(ALP_DAWN_PACKAGE_CANDIDATE "${ALP_DAWN_INSTALL_DIR}/${ALP_DAWN_LIB_DIR}/cmake/Dawn")
        if (EXISTS "${ALP_DAWN_PACKAGE_CANDIDATE}/DawnConfig.cmake")
            set(${out_var} "${ALP_DAWN_PACKAGE_CANDIDATE}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out_var} "" PARENT_SCOPE)
endfunction()

function(alp_mark_local_dawn_package)
    foreach(ALP_DAWN_MARK_CONFIG Debug Release)
        set(ALP_DAWN_MARK_DIR "${ALP_DAWN_DIR}/install/${ALP_DAWN_MARK_CONFIG}")
        if (EXISTS "${ALP_DAWN_MARK_DIR}")
            file(WRITE "${ALP_DAWN_MARK_DIR}/.alp_dawn_version" "${ALP_DAWN_VERSION}\n")
        endif()
    endforeach()
endfunction()

if (EMSCRIPTEN)

    # Ensure Dawn port exists, otherwise fetch it
    if (NOT EXISTS "${ALP_DAWN_PORT_DIR}")
        message(STATUS "Dawn port not found, fetching...")

        execute_process(
            COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_SOURCE_DIR}/misc/scripts/fetch_dawn_port.py"
                --dawn-version "${ALP_DAWN_VERSION}"
                --extern-dir "${ALP_EXTERN_DIR}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE FETCH_RESULT
        )

        if (NOT FETCH_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to fetch Dawn Emscripten port.")
        endif()
    endif()



    target_compile_options(webgpu PUBLIC
    "--use-port=sdl2"
    "--use-port=${ALP_DAWN_PORT_DIR}"
    )


    target_link_options(webgpu PUBLIC
    "-sASYNCIFY=1"
    "--use-port=sdl2"
    "--use-port=${ALP_DAWN_PORT_DIR}"
    )

else()

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(ALP_DAWN_CONFIG "Debug")
    else()
        set(ALP_DAWN_CONFIG "Release")
    endif()

    if (WIN32)
        set(ALP_DAWN_NATIVE_PLATFORM "windows-latest")
    elseif(APPLE)
        set(ALP_DAWN_NATIVE_PLATFORM "macos-latest")
    else()
        set(ALP_DAWN_NATIVE_PLATFORM "ubuntu-latest")
    endif()

    alp_find_local_dawn_package(ALP_DAWN_CMAKE_PACKAGE)

    if (NOT ALP_DAWN_CMAKE_PACKAGE)
        message(STATUS "Dawn installation missing - fetching native Dawn package...")
        execute_process(
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/misc/scripts/fetch_dawn_native.py"
                --dawn-version "${ALP_DAWN_VERSION}"
                --extern-dir "${ALP_EXTERN_DIR}"
                --build-type "${ALP_DAWN_CONFIG}"
                --platform "${ALP_DAWN_NATIVE_PLATFORM}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_FETCH_RESULT
        )

        if (NOT DAWN_FETCH_RESULT EQUAL 0)
            message(STATUS "Fetching native Dawn package failed - running install_dawn.py...")

            if (NOT DEFINED ALP_CMAKE_EXECUTABLE)
                find_program(ALP_CMAKE_EXECUTABLE cmake)
                if (NOT ALP_CMAKE_EXECUTABLE)
                    message(FATAL_ERROR "cmake not found in PATH")
                endif()
            endif()

            if (NOT DEFINED ALP_NINJA_EXECUTABLE)
                find_program(ALP_NINJA_EXECUTABLE ninja)
                if (NOT ALP_NINJA_EXECUTABLE)
                    message(FATAL_ERROR "ninja not found in PATH")
                endif()
            endif()

            execute_process(
                COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/misc/scripts/install_dawn.py"
                    --dawn-version ${ALP_DAWN_VERSION}
                    --extern-dir "${ALP_EXTERN_DIR}"
                    --cmake-path "${ALP_CMAKE_EXECUTABLE}"
                    --ninja-path "${ALP_NINJA_EXECUTABLE}"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                RESULT_VARIABLE DAWN_RESULT
            )

            if (NOT DAWN_RESULT EQUAL 0)
                message(FATAL_ERROR "install_dawn.py failed.")
            endif()
        endif()

        alp_mark_local_dawn_package()
        alp_find_local_dawn_package(ALP_DAWN_CMAKE_PACKAGE)
    endif()

    if (NOT ALP_DAWN_CMAKE_PACKAGE)
        message(FATAL_ERROR "Dawn ${ALP_DAWN_VERSION} was not found in ${ALP_DAWN_DIR}/install/${ALP_DAWN_CONFIG}.")
    endif()

    set(Dawn_DIR "${ALP_DAWN_CMAKE_PACKAGE}")
    find_package(Dawn REQUIRED CONFIG PATHS "${ALP_DAWN_CMAKE_PACKAGE}" NO_DEFAULT_PATH)
    target_link_libraries(webgpu PUBLIC dawn::webgpu_dawn)

    # Ensure SDL is installed
    if (NOT EXISTS "${ALP_SDL_INSTALL_DIR}")
        message(STATUS "SDL not found - running install_sdl.py...")

        if (NOT DEFINED ALP_CMAKE_EXECUTABLE)
            find_program(ALP_CMAKE_EXECUTABLE cmake)
            if (NOT ALP_CMAKE_EXECUTABLE)
                message(FATAL_ERROR "cmake not found")
            endif()
        endif()

        if (NOT DEFINED ALP_NINJA_EXECUTABLE)
            find_program(ALP_NINJA_EXECUTABLE ninja)
            if (NOT ALP_NINJA_EXECUTABLE)
                message(FATAL_ERROR "ninja not found")
            endif()
        endif()

        if (NOT DEFINED ALP_GIT_EXECUTABLE)
            find_program(ALP_GIT_EXECUTABLE git)
            if (NOT ALP_GIT_EXECUTABLE)
                message(FATAL_ERROR "git not found")
            endif()
        endif()

        execute_process(
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/misc/scripts/install_sdl.py"
                --install-prefix "${ALP_SDL_INSTALL_DIR}"
                --cmake-path "${ALP_CMAKE_EXECUTABLE}"
                --ninja-path "${ALP_NINJA_EXECUTABLE}"
                --git-path "${ALP_GIT_EXECUTABLE}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE SDL_RESULT
        )
        if (NOT SDL_RESULT EQUAL 0)
            message(FATAL_ERROR "install_sdl.py failed.")
        endif()
    endif()

    # find and add SDL2 (you may install it to the default install folder or ALP_SDL_INSTALL_DIR)
    # Add ALP_SDL_INSTALL_DIR to CMake's search path for find_package
    list(APPEND CMAKE_PREFIX_PATH "${ALP_SDL_INSTALL_DIR}")
    find_package(SDL2 REQUIRED)
    target_link_libraries(webgpu PUBLIC SDL2::SDL2 SDL2::SDL2main)

    # TODO verify whether this is still needed and adapt accordingly
    # If tint_lang_hlsl_writer.lib within ${ALP_DAWN_DIR}/build/debug/src exists DAWN was compiled with DX-Backends.
    file(GLOB_RECURSE TINT_LIB_PATH "${ALP_DAWN_DIR}/out/Debug/src/tint/tint_lang_hlsl_writer.lib")
    if (TINT_LIB_PATH)
        # NOTE: With Direct3D Backends we additionally need to link against dx.
        message(STATUS "Found tint_lang_hlsl_writer.lib, DAWN seems to be compiled with DX-Backends. Will link against dxguid.lib.")
        find_library(DXGUID_LIB dxguid.lib)
        if (DXGUID_LIB)
            target_link_libraries(webgpu PUBLIC ${DXGUID_LIB})
        else()
            message(FATAL_ERROR "dxguid.lib not found.")
        endif()
    endif()
endif()

# set public Dawn backend flags
target_compile_definitions(webgpu PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_DAWN WEBGPU_BACKEND_DAWN)

# --- WGSL shaders owned by the webgpu target: shared util/* helpers + the mipmap shader ---
if (ALP_ENABLE_WGSL_MINIFICATION)
    set(WEBGPU_SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    set(WEBGPU_SHADER_MINIFIED_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders_minified")
    set(MINIFY_SCRIPT "${CMAKE_SOURCE_DIR}/misc/scripts/minify_shaders.py")

    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    execute_process(
        COMMAND ${Python3_EXECUTABLE} "${MINIFY_SCRIPT}" "${WEBGPU_SHADER_SOURCE_DIR}" "${WEBGPU_SHADER_MINIFIED_DIR}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE MINIFY_RESULT
    )
    if(NOT MINIFY_RESULT EQUAL 0)
        message(FATAL_ERROR "Shader minification failed")
    endif()

    file(GLOB_RECURSE WEBGPU_SHADER_SOURCES "${WEBGPU_SHADER_SOURCE_DIR}/*.wgsl")
    add_custom_target(minify_webgpu_shaders
        COMMAND ${Python3_EXECUTABLE} "${MINIFY_SCRIPT}" "${WEBGPU_SHADER_SOURCE_DIR}" "${WEBGPU_SHADER_MINIFIED_DIR}"
        DEPENDS ${WEBGPU_SHADER_SOURCES} ${MINIFY_SCRIPT}
        COMMENT "Minifying webgpu shaders"
        VERBATIM
    )
    add_dependencies(webgpu minify_webgpu_shaders)

    set(WEBGPU_SHADER_BASE_DIR "${WEBGPU_SHADER_MINIFIED_DIR}")
else()
    set(WEBGPU_SHADER_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
endif()

qt_add_resources(webgpu "webgpu_shaders"
    PREFIX "/shaders/webgpu"
    BASE "${WEBGPU_SHADER_BASE_DIR}"
    FILES
        "${WEBGPU_SHADER_BASE_DIR}/mipmap.wgsl"
        "${WEBGPU_SHADER_BASE_DIR}/snow.wgsl"
        "${WEBGPU_SHADER_BASE_DIR}/general.wgsl"
        "${WEBGPU_SHADER_BASE_DIR}/tile_util.wgsl"
        "${WEBGPU_SHADER_BASE_DIR}/normals_util.wgsl"
        "${WEBGPU_SHADER_BASE_DIR}/hashing.wgsl"
        "${WEBGPU_SHADER_BASE_DIR}/encoder.wgsl"
        "${WEBGPU_SHADER_BASE_DIR}/filtering.wgsl"
        "${WEBGPU_SHADER_BASE_DIR}/noise.wgsl"
    )

target_compile_definitions(webgpu PUBLIC ALP_SHADER_DIR_WEBGPU="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")

target_link_libraries(webgpu PUBLIC Qt::Core nucleus)
target_include_directories(webgpu PRIVATE .)
