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


get_filename_component(CURRENT_SCRIPT_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)

include(${CURRENT_SCRIPT_DIR}/Version.cmake)


get_filename_component(ALP_VERSION_TEMPLATE_DIR ${ALP_VERSION_TEMPLATE} DIRECTORY)

alp_generate_version_file(
    GIT_DIR             "${ALP_VERSION_TEMPLATE_DIR}"
    VERSION_TEMPLATE    "${ALP_VERSION_TEMPLATE}"
    VERSION_DESTINATION "${ALP_VERSION_DESTINATION}"
)
