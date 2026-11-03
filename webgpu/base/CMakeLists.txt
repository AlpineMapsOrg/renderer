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
    RenderGraph.h RenderGraph.cpp RenderGraph_internal.h

    webgpu_interface.hpp webgpu_interface.cpp
)

add_library(webgpu STATIC ${SOURCES})

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

target_compile_definitions(webgpu
    PUBLIC ALP_SHADER_DIR_WEBGPU="${CMAKE_CURRENT_SOURCE_DIR}/shaders/"
    PRIVATE WEBGPU_BACKEND_DAWN
)

target_link_libraries(webgpu PUBLIC
    Qt::Core
    nucleus
    alp_webgpu_api
    alp_sdl2_platform
)
target_include_directories(webgpu PRIVATE .)
