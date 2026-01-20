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

set(ALP_DAWN_VERSION "20251204.180718") # Version of Dawn to download (see https://github.com/google/dawn/releases)
set(ALP_DAWN_DIR "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/dawn")
set(ALP_SDL_INSTALL_DIR "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/sdl")
set(ALP_DAWN_PORT_DIR "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/emdawnwebgpu_pkg/emdawnwebgpu.port.py")

set(SOURCES

    raii/base_types.h
    raii/Texture.h raii/Texture.cpp
    raii/TextureWithSampler.h raii/TextureWithSampler.cpp
    raii/RawBuffer.h
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

    webgpu_interface.hpp webgpu_interface.cpp)

add_library(webgpu STATIC ${SOURCES})

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if (EMSCRIPTEN)

    # Ensure Dawn port exists, otherwise fetch it
    if (NOT EXISTS "${ALP_DAWN_PORT_DIR}")
        message(STATUS "Dawn port not found, fetching...")

        execute_process(
            COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_SOURCE_DIR}/scripts/fetch_dawn_port.py"
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

    # Ensure Dawn is installed
    if (NOT EXISTS "${ALP_DAWN_DIR}/install/${CMAKE_BUILD_TYPE}/lib/cmake/Dawn")
        message(STATUS "Dawn installation missing - running install_dawn.py...")

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
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/install_dawn.py"
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



    set(Dawn_DIR "${ALP_DAWN_DIR}/install/${CMAKE_BUILD_TYPE}/lib/cmake/Dawn")
    find_package(Dawn REQUIRED)
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
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/install_sdl.py"
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

# set public IMGUI_IMPL_WEBGPU_BACKEND_DAWN flag
target_compile_definitions(webgpu PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_DAWN)

target_link_libraries(webgpu PUBLIC Qt::Core nucleus)
target_include_directories(webgpu PRIVATE .)
