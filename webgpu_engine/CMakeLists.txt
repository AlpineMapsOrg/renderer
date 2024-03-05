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

project(alpine-renderer-webgpu_engine LANGUAGES C CXX)

# option to enable/disable imgui
option(ALP_WEBGPU_APP_ENABLE_IMGUI "Enable imgui" ON)


set(SOURCES
    webgpu.hpp webgpu.cpp
    dawn_setup.h dawn_setup.cpp
    Window.h Window.cpp
    Test.h Test.cpp
)

qt_add_library(webgpu_engine STATIC ${SOURCES})

qt_add_resources(webgpu_engine "shaders"
    PREFIX "/wgsl_shaders"
    BASE "wgsl_shaders/"
    FILES
        "wgsl_shaders/DebugTriangle.wgsl"
)

target_link_libraries(webgpu_engine PUBLIC nucleus Qt::Core)
target_include_directories(webgpu_engine PRIVATE .)

if (EMSCRIPTEN)

else()
    alp_add_git_repository(glfw URL https://github.com/AlpineMapsOrgDependencies/glfw_slim.git COMMITISH 1c340247f42f8dc9ed3f971eb292ce76be6077e5)

    include(${CMAKE_SOURCE_DIR}/cmake/alp_target_add_dawn.cmake)

    # creates new interface called webgpu and adds dawn libraries to it
    add_library(webgpu INTERFACE)
    alp_target_add_dawn(webgpu "${ALP_DAWN_DIR}" INTERFACE)

    target_link_libraries(webgpu_engine PUBLIC webgpu)
    #alp_target_add_dawn(webgpu_app "${ALP_DAWN_DIR}" PRIVATE)

endif()

if (ALP_WEBGPU_APP_ENABLE_IMGUI)
    alp_add_git_repository(imgui URL https://github.com/AlpineMapsOrgDependencies/imgui_slim.git COMMITISH f738c335f73956ed3715434987e6f82bac442e87 )
    target_link_libraries(webgpu_engine PUBLIC imgui)
    target_compile_definitions(webgpu_engine PUBLIC "ALP_WEBGPU_APP_ENABLE_IMGUI")
endif()

target_compile_definitions(webgpu_engine PUBLIC ALP_RESOURCES_PREFIX="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
