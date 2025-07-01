#############################################################################
# AlpineMaps.org
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

include_guard(GLOBAL)


# Function to check if a local CMake script specified by file_path
# differs from the version in the central AlpineMapsOrg/cmake_scripts repository.
#
# Arguments:
#   file_path: Path to the CMake script


if(NOT COMMAND alp_add_git_repository)
    include(${CMAKE_CURRENT_LIST_DIR}/AddRepo.cmake)
endif()

if(NOT DEFINED _alp_check_for_script_updates_repo_flag)
    set_property(GLOBAL PROPERTY _alp_check_for_script_updates_repo_flag FALSE)
endif()

function(alp_check_for_script_updates script_path)
    get_filename_component(script_name "${script_path}" NAME)

    get_property(_repo_done GLOBAL PROPERTY _alp_check_for_script_updates_repo_flag)
    if(_repo_done)
        get_property(cmake_scripts_SOURCE_DIR GLOBAL PROPERTY _alp_check_for_script_updates_script_repo)
    else()
        alp_add_git_repository(cmake_scripts
                               URL "https://github.com/AlpineMapsOrg/cmake_scripts.git"
                               COMMITISH origin/main
                               DO_NOT_ADD_SUBPROJECT PRIVATE_DO_NOT_CHECK_FOR_SCRIPT_UPDATES)
        set_property(GLOBAL PROPERTY _alp_check_for_script_updates_repo_flag TRUE)
        set_property(GLOBAL PROPERTY _alp_check_for_script_updates_script_repo ${cmake_scripts_SOURCE_DIR})
    endif()

    set(external_script_path "${cmake_scripts_SOURCE_DIR}/${script_name}")
    get_filename_component(absolute_script_path "${script_path}" REALPATH)

    if(NOT EXISTS "${absolute_script_path}")
        message(WARNING "alp_check_for_cmake_script_updates: Local script '${script_path}' (${absolute_script_path}) does not exist. Cannot compare.")
        return()
    endif()

    if(NOT EXISTS "${external_script_path}")
        message(WARNING "alp_check_for_cmake_script_updates: Corresponding external script '${external_script_path}' does not exist in the 'cmake_scripts' repository. Cannot compare.")
        return()
    endif()

    file(READ "${script_path}" local_content)
    file(READ "${external_script_path}" external_content)

    if(NOT local_content STREQUAL external_content)
        message(WARNING "alp_check_for_cmake_script_updates: The local script '${script_path}' has different content compared to the version in the central 'cmake_scripts' repository ('${external_script_path}'). Consider updating the local file or the repository.")
    else()
        message(STATUS "alp_check_for_cmake_script_updates: Local script '${script_path}' is up-to-date.")
    endif()
endfunction()

alp_check_for_script_updates("${CMAKE_CURRENT_LIST_FILE}")

