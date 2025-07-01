#############################################################################
# Alpine Terrain Renderer
# Copyright (C) 2025 Adam Celarek <family name at cg tuwien ac at>
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

cmake_minimum_required(VERSION 3.25)

# Expect exactly two arguments: BUILD_DIR and SOURCE_DIR
if (NOT DEFINED BUILD_DIR OR NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "Usage: cmake -DBUILD_DIR=<BUILD_DIR> -DSOURCE_DIR=<SOURCE_DIR> -P alp_fix_qmldir_for_hotreload.cmake")
endif()

# Recursively find all files named 'qmldir' in BUILD_DIR
file(GLOB_RECURSE QMLDIR_FILES
    "${BUILD_DIR}/qmldir"
)

foreach(qmldir_file IN LISTS QMLDIR_FILES)
    # Read the file line by line
    file(STRINGS "${qmldir_file}" file_lines)

    # Prepare a new list for the modified lines
    set(new_lines "")
    foreach(line IN LISTS file_lines)
        # For lines starting with "prefer ", replace with "prefer ${SOURCE_DIR}"
        string(REGEX REPLACE "^prefer .*" "prefer ${SOURCE_DIR}/" new_line "${line}")
        list(APPEND new_lines "${new_line}")
    endforeach()

    # Join modified lines with newlines and write them back
    string(REPLACE ";" "\n" joined_content "${new_lines}")
    file(WRITE "${qmldir_file}" "${joined_content}\n")
endforeach()
