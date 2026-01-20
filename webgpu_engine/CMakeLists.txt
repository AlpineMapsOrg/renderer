#############################################################################
# Alpine Terrain Renderer
# Copyright (C) 2024 Adam Celarek <family name at cg tuwien ac at>
# Copyright (C) 2024 Gerald Kimmersdorfer
# Copyright (C) 2024 Patrick Komon
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

# option to enable/disable imgui
option(ALP_WEBGPU_APP_ENABLE_IMGUI "Enable imgui" ON)
option(ALP_ENABLE_WGSL_MINIFICATION "Minify WGSL shaders to reduce size" OFF)

project(alpine-renderer-webgpu_engine LANGUAGES C CXX)


set(SOURCES
    Window.h Window.cpp
    PipelineSettings.h PipelineSettings.cpp
    ShaderModuleManager.h ShaderModuleManager.cpp
    PipelineManager.h PipelineManager.cpp
    UniformBufferObjects.h UniformBufferObjects.cpp
    TileGeometry.h TileGeometry.cpp
    Buffer.h Buffer.cpp
    TrackRenderer.h TrackRenderer.cpp
    Context.h Context.cpp

    compute/GpuHashMap.h compute/GpuHashMap.cpp
    compute/GpuTileStorage.h compute/GpuTileStorage.cpp
    compute/GpuTileId.h compute/GpuTileId.cpp
    compute/RectangularTileRegion.h compute/RectangularTileRegion.cpp
    compute/nodes/Node.h compute/nodes/Node.cpp
    compute/nodes/SelectTilesNode.h compute/nodes/SelectTilesNode.cpp
    compute/nodes/RequestTilesNode.h compute/nodes/RequestTilesNode.cpp
    compute/nodes/CreateHashMapNode.h compute/nodes/CreateHashMapNode.cpp
    compute/nodes/ComputeNormalsNode.h compute/nodes/ComputeNormalsNode.cpp
    compute/nodes/DownsampleTilesNode.h compute/nodes/DownsampleTilesNode.cpp
    compute/nodes/NodeGraph.h compute/nodes/NodeGraph.cpp
    compute/nodes/ComputeSnowNode.h compute/nodes/ComputeSnowNode.cpp
    compute/nodes/UpsampleTexturesNode.h compute/nodes/UpsampleTexturesNode.cpp
    compute/nodes/ComputeAvalancheTrajectoriesNode.h compute/nodes/ComputeAvalancheTrajectoriesNode.cpp
    compute/nodes/ComputeAvalancheInfluenceAreaNode.h compute/nodes/ComputeAvalancheInfluenceAreaNode.cpp
    compute/nodes/ComputeD8DirectionsNode.h compute/nodes/ComputeD8DirectionsNode.cpp
    compute/nodes/TileExportNode.h compute/nodes/TileExportNode.cpp
    compute/nodes/BufferExportNode.h compute/nodes/BufferExportNode.cpp
    compute/nodes/BufferToTextureNode.h compute/nodes/BufferToTextureNode.cpp
    compute/nodes/ComputeReleasePointsNode.h compute/nodes/ComputeReleasePointsNode.cpp
    compute/nodes/TileStitchNode.h compute/nodes/TileStitchNode.cpp
    compute/nodes/HeightDecodeNode.h compute/nodes/HeightDecodeNode.cpp
    compute/nodes/FxaaNode.h compute/nodes/FxaaNode.cpp
    compute/nodes/IterativeSimulationNode.h compute/nodes/IterativeSimulationNode.cpp
    compute/nodes/LoadTextureNode.h compute/nodes/LoadTextureNode.cpp
    compute/nodes/LoadRegionAabbNode.h compute/nodes/LoadRegionAabbNode.cpp
    compute/nodes/util.h compute/nodes/util.cpp
)

# NodeGraphRenderer and NodeRenderer are only compiled when ImGui is enabled
if (ALP_WEBGPU_APP_ENABLE_IMGUI)
    list(APPEND SOURCES
        compute/NodeGraphRenderer.h compute/NodeGraphRenderer.cpp
        compute/NodeRenderer.h compute/NodeRenderer.cpp
    )
endif()

qt_add_library(webgpu_engine STATIC ${SOURCES})

if (ALP_ENABLE_WGSL_MINIFICATION)
    set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/wgsl_shaders")
    set(SHADER_MINIFIED_DIR "${CMAKE_CURRENT_BINARY_DIR}/wgsl_shaders_minified")
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
    add_custom_target(minify_shaders
        COMMAND ${Python3_EXECUTABLE} "${MINIFY_SCRIPT}" "${SHADER_SOURCE_DIR}" "${SHADER_MINIFIED_DIR}"
        DEPENDS ${SHADER_SOURCES} ${MINIFY_SCRIPT}
        COMMENT "Minifying shaders"
        VERBATIM
    )
    add_dependencies(webgpu_engine minify_shaders)

    set(SHADER_BASE_DIR "${SHADER_MINIFIED_DIR}")
    target_compile_definitions(webgpu_engine PUBLIC ALP_ENABLE_WGSL_MINIFICATION)
else()
    set(SHADER_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/wgsl_shaders")
endif()

qt_add_resources(webgpu_engine "shaders"
    PREFIX "/wgsl_shaders"
    BASE "${SHADER_BASE_DIR}"
    FILES
        "${SHADER_BASE_DIR}/render_tiles.wgsl"
        "${SHADER_BASE_DIR}/render_atmosphere.wgsl"
        "${SHADER_BASE_DIR}/render_lines.wgsl"
        "${SHADER_BASE_DIR}/compose_pass.wgsl"
        "${SHADER_BASE_DIR}/screen_pass_vert.wgsl"

        "${SHADER_BASE_DIR}/compute/downsample_compute.wgsl"
        "${SHADER_BASE_DIR}/compute/normals_compute.wgsl"
        "${SHADER_BASE_DIR}/compute/snow_compute.wgsl"
        "${SHADER_BASE_DIR}/compute/upsample_textures_compute.wgsl"
        "${SHADER_BASE_DIR}/compute/avalanche_trajectories_compute.wgsl"
        "${SHADER_BASE_DIR}/compute/buffer_to_texture_compute.wgsl"
        "${SHADER_BASE_DIR}/compute/avalanche_influence_area_compute.wgsl"
        "${SHADER_BASE_DIR}/compute/d8_compute.wgsl"
        "${SHADER_BASE_DIR}/compute/mipmap_creation_compute.wgsl"
        "${SHADER_BASE_DIR}/compute/compute_release_points.wgsl"
        "${SHADER_BASE_DIR}/compute/height_decode_compute.wgsl"
        "${SHADER_BASE_DIR}/compute/fxaa_compute.wgsl"
        "${SHADER_BASE_DIR}/compute/iterative_simulation_compute.wgsl"

        "${SHADER_BASE_DIR}/util/atmosphere.wgsl"
        "${SHADER_BASE_DIR}/util/snow.wgsl"
        "${SHADER_BASE_DIR}/util/tile_util.wgsl"
        "${SHADER_BASE_DIR}/util/normals_util.wgsl"
        "${SHADER_BASE_DIR}/util/tile_hashmap.wgsl"
        "${SHADER_BASE_DIR}/util/shared_config.wgsl"
        "${SHADER_BASE_DIR}/util/camera_config.wgsl"
        "${SHADER_BASE_DIR}/util/hashing.wgsl"
        "${SHADER_BASE_DIR}/util/encoder.wgsl"
        "${SHADER_BASE_DIR}/util/color_mapping.wgsl"
        "${SHADER_BASE_DIR}/util/filtering.wgsl"
        "${SHADER_BASE_DIR}/util/noise.wgsl"
        "${SHADER_BASE_DIR}/util/random.wgsl"
    )

# Note: This has to go into the webgpu_app as soon as we don't use ImGui in the engine anymore!!!
# We can't do it right now as the engine is not dependent on the app. so we dont have acess in this case.
if (ALP_WEBGPU_APP_ENABLE_IMGUI)
    # For Font Awesome Icon Headers:
    alp_add_git_repository(iconfontcppheaders URL https://github.com/juliettef/IconFontCppHeaders.git COMMITISH f30b1e73b2d71eb331d77619c3f1de34199afc38 DO_NOT_ADD_SUBPROJECT)
    target_include_directories(webgpu_engine PUBLIC ../extern/iconfontcppheaders)

    alp_add_git_repository(imgui URL https://github.com/AlpineMapsOrgDependencies/imgui_slim.git COMMITISH 4d8ecbf58ebf32e757ff56a60f1fb0446033edb8)

    set(IMNODES_IMGUI_TARGET_NAME imgui)
    alp_add_git_repository(imnodes URL https://github.com/AlpineMapsOrgDependencies/imnodes_slim.git COMMITISH 324355e64ed8b5bea02fe1439cdcb5c7773b4436)

    alp_add_git_repository(ImGuiFileDialog URL https://github.com/AlpineMapsOrgDependencies/ImGuiFileDialog_slim.git COMMITISH 39f98eb131ee00d476f1002336fc7e9e1795ccc5)
    target_link_libraries(ImGuiFileDialog PRIVATE imgui)

    target_link_libraries(webgpu_engine PUBLIC imgui imnodes ImGuiFileDialog)
    target_compile_definitions(webgpu_engine PUBLIC "ALP_WEBGPU_APP_ENABLE_IMGUI")
endif()

target_link_libraries(webgpu_engine PUBLIC nucleus Qt::Core webgpu)
target_include_directories(webgpu_engine PRIVATE .)

target_compile_definitions(webgpu_engine PUBLIC ALP_RESOURCES_PREFIX="${CMAKE_CURRENT_SOURCE_DIR}/wgsl_shaders/")
