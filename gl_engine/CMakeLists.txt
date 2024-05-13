#############################################################################
# Alpine Terrain Renderer
# Copyright (C) 2023 Adam Celarek <family name at cg tuwien ac at>
# Copyright (C) 2023 Gerald Kimmersdorfer
# Copyright (C) 2024 Lucas Dworschak
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

project(alpine-renderer-gl_engine LANGUAGES CXX)

# If true the shaders will be loaded from the given ALP_SHADER_NETWORK_URL and can be reloaded
# inside the APP without the need for recompilation. You have to serve the files yourself. There is no HTTP-Server!
option(ALP_ENABLE_SHADER_NETWORK_HOTRELOAD "reload shader files per HTTP request. Allows Hot-Reload on Android/WebGL" OFF)
set(ALP_SHADER_NETWORK_URL "http://localhost:5500/" CACHE STRING "url location, which points to the shaders directory for network hot reload")

qt_add_library(gl_engine STATIC
    Framebuffer.h Framebuffer.cpp
    ShaderManager.h ShaderManager.cpp
    TileManager.h TileManager.cpp
    Window.cpp Window.h
    helpers.h
    ShaderProgram.h ShaderProgram.cpp
    UniformBufferObjects.h UniformBufferObjects.cpp
    UniformBuffer.h UniformBuffer.cpp
    SSAO.h SSAO.cpp
    ShadowMapping.h ShadowMapping.cpp
    GpuAsyncQueryTimer.h GpuAsyncQueryTimer.cpp
    MapLabelManager.h MapLabelManager.cpp
    Texture.h Texture.cpp
)
target_link_libraries(gl_engine PUBLIC nucleus Qt::OpenGL)
target_include_directories(gl_engine PRIVATE .)

qt_add_resources(gl_engine "shaders"
    PREFIX "/gl_shaders"
    BASE "shaders/"
    FILES
    shaders/atmosphere_bg.frag
    shaders/atmosphere_implementation.glsl
    shaders/screen_copy.frag
    shaders/screen_pass.vert
    shaders/tile.frag
    shaders/tile.vert
    shaders/encoder.glsl
    shaders/compose.frag
    shaders/shared_config.glsl
    shaders/camera_config.glsl
    shaders/hashing.glsl
    shaders/ssao.frag
    shaders/ssao_blur.frag
    shaders/shadowmap.vert
    shaders/shadowmap.frag
    shaders/shadow_config.glsl
    shaders/overlay_steepness.glsl
    shaders/labels.frag
    shaders/labels.vert
    shaders/snow.glsl
    shaders/tile.glsl
    shaders/tile_id.glsl
)
target_compile_definitions(gl_engine PUBLIC ALP_RESOURCES_PREFIX="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")

if(ALP_ENABLE_SHADER_NETWORK_HOTRELOAD)
    target_compile_definitions(gl_engine PUBLIC ALP_ENABLE_SHADER_NETWORK_HOTRELOAD=true)
else()
    target_compile_definitions(gl_engine PUBLIC ALP_ENABLE_SHADER_NETWORK_HOTRELOAD=false)
endif()

target_compile_definitions(gl_engine PUBLIC "ALP_SHADER_NETWORK_URL=\"${ALP_SHADER_NETWORK_URL}\"")
