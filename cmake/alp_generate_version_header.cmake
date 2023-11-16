#############################################################################
# Alpine Terrain Renderer
# Copyright (C) 2023 Adam Celarek <family name at cg tuwien ac at>
# Copyright (C) 2015 Taylor Braun-Jones (via github.com/nocnokneo/cmake-git-versioning-example)
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
 
find_package(Git 2.22 REQUIRED)

get_filename_component(SRC_DIR ${SRC} DIRECTORY)

execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --dirty=-d --abbrev=1
    WORKING_DIRECTORY ${SRC_DIR}
    RESULT_VARIABLE git_version_result
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE ALP_VERSION)

if (${git_version_result})
    message(WARNING "Retrieving version string from git was not successfull. Setting it to 'vUnknown'")
    set(${output_variable} "vUnknown" PARENT_SCOPE)
else()
    string(REPLACE "-" "." ALP_VERSION ${ALP_VERSION})
endif()

configure_file(${SRC} ${DST} @ONLY)
