#############################################################################
# weBIGeo
# Copyright (C) 2026 Gerald Kimmersdorfer
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

project(alpine-renderer-webgpu_compute LANGUAGES C CXX)

set(SOURCES
    GpuTileStorage.h GpuTileStorage.cpp
    RectangularTileRegion.h RectangularTileRegion.cpp
    GraphRunContext.h
    NodeGraph.h NodeGraph.cpp
    NodeRegistry.h NodeRegistry.cpp

    nodes/Node.h nodes/Node.cpp
    nodes/SelectTilesNode.h nodes/SelectTilesNode.cpp
    nodes/RequestTilesNode.h nodes/RequestTilesNode.cpp
    nodes/ComputeNormalsNode.h nodes/ComputeNormalsNode.cpp
    nodes/ComputeSnowNode.h nodes/ComputeSnowNode.cpp
    nodes/UpsampleTexturesNode.h nodes/UpsampleTexturesNode.cpp
    nodes/ComputeAvalancheTrajectoriesNode.h nodes/ComputeAvalancheTrajectoriesNode.cpp
    nodes/ExportNode.h nodes/ExportNode.cpp
    nodes/GPXTrackNode.h nodes/GPXTrackNode.cpp
    nodes/BufferToTextureNode.h nodes/BufferToTextureNode.cpp
    nodes/ComputeReleasePointsNode.h nodes/ComputeReleasePointsNode.cpp
    nodes/TileStitchNode.h nodes/TileStitchNode.cpp
    nodes/HeightDecodeNode.h nodes/HeightDecodeNode.cpp
    nodes/IterativeSimulationNode.h nodes/IterativeSimulationNode.cpp
    nodes/LoadTextureNode.h nodes/LoadTextureNode.cpp
    nodes/util.h nodes/util.cpp
)

qt_add_library(webgpu_compute STATIC ${SOURCES})

if (ALP_ENABLE_WGSL_MINIFICATION)
    set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    set(SHADER_MINIFIED_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders_minified")
    set(MINIFY_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/minify_shaders.py")

    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    execute_process(
        COMMAND ${Python3_EXECUTABLE} "${MINIFY_SCRIPT}" "${SHADER_SOURCE_DIR}" "${SHADER_MINIFIED_DIR}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE MINIFY_RESULT
    )
    if(NOT MINIFY_RESULT EQUAL 0)
        message(FATAL_ERROR "Shader minification failed")
    endif()

    file(GLOB_RECURSE SHADER_SOURCES "${SHADER_SOURCE_DIR}/*.wgsl")
    add_custom_target(minify_webgpu_compute_shaders
        COMMAND ${Python3_EXECUTABLE} "${MINIFY_SCRIPT}" "${SHADER_SOURCE_DIR}" "${SHADER_MINIFIED_DIR}"
        DEPENDS ${SHADER_SOURCES} ${MINIFY_SCRIPT}
        COMMENT "Minifying webgpu_compute shaders"
        VERBATIM
    )
    add_dependencies(webgpu_compute minify_webgpu_compute_shaders)

    set(SHADER_BASE_DIR "${SHADER_MINIFIED_DIR}")
else()
    set(SHADER_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
endif()

qt_add_resources(webgpu_compute "webgpu_compute_shaders"
    PREFIX "/shaders/webgpu_compute"
    BASE "${SHADER_BASE_DIR}"
    FILES
        "${SHADER_BASE_DIR}/normals_compute.wgsl"
        "${SHADER_BASE_DIR}/snow_compute.wgsl"
        "${SHADER_BASE_DIR}/upsample_textures_compute.wgsl"
        "${SHADER_BASE_DIR}/avalanche_trajectories_compute.wgsl"
        "${SHADER_BASE_DIR}/buffer_to_texture_compute.wgsl"
        "${SHADER_BASE_DIR}/compute_release_points.wgsl"
        "${SHADER_BASE_DIR}/height_decode_compute.wgsl"
        "${SHADER_BASE_DIR}/iterative_simulation_compute.wgsl"

        "${SHADER_BASE_DIR}/tile_hashmap.wgsl"
        "${SHADER_BASE_DIR}/color_mapping.wgsl"
        "${SHADER_BASE_DIR}/random.wgsl"
    )

target_link_libraries(webgpu_compute PUBLIC nucleus Qt::Core webgpu)
target_include_directories(webgpu_compute PRIVATE .)

target_compile_definitions(webgpu_compute PUBLIC ALP_SHADER_DIR_WEBGPU_COMPUTE="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
