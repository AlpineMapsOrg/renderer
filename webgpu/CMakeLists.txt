#############################################################################
# weBIGeo
# Copyright (C) 2024 Gerald Kimmersdorfer
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

# set INCLUDE_DIR based on EMSCRIPTEN
if (EMSCRIPTEN)
    set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/emscripten/include)
else()
    set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dawn/include)
endif()

set(SOURCES
    ${INCLUDE_DIR}/webgpu.hpp webgpu.cpp
    webgpu_interface.hpp webgpu_interface.cpp)

add_library(webgpu STATIC ${SOURCES})

if (EMSCRIPTEN)

else()
    # add dawn libraries to webgpu library
    include(${CMAKE_SOURCE_DIR}/cmake/alp_target_add_dawn.cmake)
    alp_target_add_dawn(webgpu "${ALP_DAWN_DIR}" PUBLIC)

    # add glfw to webgpu library
    alp_add_git_repository(glfw URL https://github.com/AlpineMapsOrgDependencies/glfw_slim.git COMMITISH 1c340247f42f8dc9ed3f971eb292ce76be6077e5)
    target_link_libraries(webgpu PUBLIC glfw)
endif()

target_include_directories(webgpu PUBLIC ${INCLUDE_DIR})
