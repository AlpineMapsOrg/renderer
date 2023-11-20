#############################################################################
# Alpine Terrain Renderer
# Copyright (C) 2023 Adam Celarek <family name at cg tuwien ac at>
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

cmake_minimum_required(VERSION 3.24)
project(alpine-renderer-unittests LANGUAGES CXX)

alp_add_git_repository(qml_catch2_console URL https://github.com/AlpineMapsOrg/qml_catch2_console.git COMMITISH origin/main DO_NOT_ADD_SUBPROJECT)
include(${CMAKE_SOURCE_DIR}/cmake/alp_add_unittest.cmake)

add_subdirectory(nucleus)
add_subdirectory(gl_engine)
