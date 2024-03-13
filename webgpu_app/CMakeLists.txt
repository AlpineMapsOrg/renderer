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

project(alpine-renderer-webgpu_app LANGUAGES C CXX)

qt_add_executable(webgpu_app
    resources.qrc
    main.cpp
    glfw3webgpu.h glfw3webgpu.c
    TerrainRenderer.h TerrainRenderer.cpp
)

set_target_properties(webgpu_app PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# TODO: Get rid of Qt::GUI (no QImage)
target_link_libraries(webgpu_app PUBLIC webgpu_engine Qt::Core Qt::Gui)
target_link_libraries(webgpu_app PRIVATE glfw)
target_include_directories(webgpu_app PRIVATE .)

qt_add_resources(webgpu_app "height_data"
    PREFIX "/map"
    BASE ${renderer_static_data_SOURCE_DIR}
    FILES ${renderer_static_data_SOURCE_DIR}/height_data.atb
)
