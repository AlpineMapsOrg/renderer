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

function(alp_get_version)
    # Usage:
    #   alp_get_version(
    #       GIT_DIR         <path to git repo>
    #       VERSION_VAR     <name of var to hold the version string>
    #       VERSION_INT_VAR <name of var to hold the commit count, optional>
    #   )

    cmake_parse_arguments(ALP "" "GIT_DIR;VERSION_VAR;VERSION_INT_VAR" "" ${ARGN})

    if (NOT ALP_GIT_DIR)
        message(FATAL_ERROR "alp_get_version() requires GIT_DIR to be specified")
    endif()

    if (NOT ALP_VERSION_VAR)
        message(FATAL_ERROR "alp_get_version() requires ALP_VERSION_VAR to be specified")
    endif()

    find_package(Git 2.22 REQUIRED)

    #--- Retrieve the version string (e.g. "v1.2.3-dirty") ---
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty=-d --abbrev=1
        WORKING_DIRECTORY ${ALP_GIT_DIR}
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE _local_version
    )

    if (_local_version STREQUAL "")
        message(WARNING "Retrieving version string from git failed; using 'vUnknown'")
        set(_local_version "vUnknown")
    else()
        string(REPLACE "-g" "." _local_version ${_local_version})
        string(REPLACE "-"   "." _local_version ${_local_version})
    endif()
    set(${ALP_VERSION_VAR} "${_local_version}" PARENT_SCOPE)

    #--- Retrieve number of commits (rev-list HEAD --count) ---
    if (ALP_VERSION_INT_VAR)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list HEAD --count
            WORKING_DIRECTORY ${ALP_GIT_DIR}
            OUTPUT_STRIP_TRAILING_WHITESPACE
            OUTPUT_VARIABLE _local_version_integer
        )

        if (_local_version_integer STREQUAL "")
            message(WARNING "Retrieving number of commits from git failed; using '0'")
            set(_local_version_integer "0")
        endif()

        set(${ALP_VERSION_INT_VAR}   "${_local_version_integer}" PARENT_SCOPE)
    endif()
endfunction()


function(alp_generate_version_file)
    # Usage:
    #   alp_generate_version_file(
    #       GIT_DIR             <path to git repo>
    #       VERSION_TEMPLATE    <path to .in template file>
    #       VERSION_DESTINATION <destination file path to generate>
    #   )

    cmake_parse_arguments(ALP ""
        "GIT_DIR;VERSION_TEMPLATE;VERSION_DESTINATION"
        ""
        ${ARGN}
    )

    if (NOT ALP_GIT_DIR)
        message(FATAL_ERROR "alp_generate_version_file() requires GIT_DIR")
    endif()
    if (NOT ALP_VERSION_TEMPLATE)
        message(FATAL_ERROR "alp_generate_version_file() requires VERSION_TEMPLATE")
    endif()
    if (NOT ALP_VERSION_DESTINATION)
        message(FATAL_ERROR "alp_generate_version_file() requires VERSION_DESTINATION")
    endif()

    # Retrieve version info
    alp_get_version(
        GIT_DIR         "${ALP_GIT_DIR}"
        VERSION_VAR     "_version"
    )

    # Because ALP_VERSION_VAR is a *name of a variable*, we use indirect expansion:
    set(ALP_VERSION         "${_version}")

    # Use configure_file() to generate your output
    configure_file(
        "${ALP_VERSION_TEMPLATE}"
        "${ALP_VERSION_DESTINATION}"
        @ONLY
    )
endfunction()

