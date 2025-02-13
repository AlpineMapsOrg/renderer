#############################################################################
# Alpine Radix
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

find_package(Git 2.22 REQUIRED)

# CMake's FetchContent caches information about the downloads / clones in the build dir.
# Therefore it walks over the clones every time we switch the build type (release, debug, webassembly, android etc),
# which takes forever. Moreover, it messes up changes to subprojects. This function, on the other hand, checks whether
# we are on a branch and in that case only issues a warning. Use origin/main or similar, if you want to stay up-to-date
# with upstream.

function(alp_add_git_repository name)
    set(options DO_NOT_ADD_SUBPROJECT NOT_SYSTEM)
    set(oneValueArgs URL COMMITISH DESTINATION_PATH)
    set(multiValueArgs )
    cmake_parse_arguments(PARSE_ARGV 1 PARAM "${options}" "${oneValueArgs}" "${multiValueArgs}")

    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR} )
    set(repo_dir ${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/${name})
    set(short_repo_dir ${ALP_EXTERN_DIR}/${name})
    if (DEFINED PARAM_DESTINATION_PATH AND NOT PARAM_DESTINATION_PATH STREQUAL "")
        set(repo_dir ${CMAKE_SOURCE_DIR}/${PARAM_DESTINATION_PATH})
        set(short_repo_dir ${PARAM_DESTINATION_PATH})
    endif()

    set(${name}_SOURCE_DIR "${repo_dir}" PARENT_SCOPE)

    if(EXISTS "${repo_dir}/.git")
        message(STATUS "Updating git repo in ${short_repo_dir}")

        # Check internet connection
        execute_process(
            COMMAND ${GIT_EXECUTABLE} ls-remote ${PARAM_URL}
            OUTPUT_QUIET
            ERROR_QUIET
            RESULT_VARIABLE GIT_LSREMOTE_RESULT
        )

        # First, see if PARAM_COMMITISH is a valid local ref at all:
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --verify ${PARAM_COMMITISH}
            WORKING_DIRECTORY ${repo_dir}
            OUTPUT_VARIABLE GIT_COMMIT_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE GIT_COMMIT_RESULT
        )

        if (NOT GIT_COMMIT_RESULT)
            #
            # At this point, PARAM_COMMITISH is recognized by Git
            # (could be a tag (lightweight or annotated) or a direct commit SHA).
            # The problem: if it's an *annotated* tag, rev-parse gives us
            # the tag object's hash, not the commit hash.
            #
            # => Force resolve the actual commit object with ^{commit}:
            #
            execute_process(
                COMMAND ${GIT_EXECUTABLE} rev-parse --verify ${PARAM_COMMITISH}^{commit}
                WORKING_DIRECTORY ${repo_dir}
                OUTPUT_VARIABLE GIT_COMMIT_OBJECT
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE GIT_COMMIT_OBJECT_RESULT
            )

            if (GIT_COMMIT_OBJECT_RESULT EQUAL 0)
                # Successfully resolved a commit object
                set(CHECK_COMMITISH "${GIT_COMMIT_OBJECT}")
            else()
                # Fallback if that fails (should rarely happen if it's a proper commit/tag)
                set(CHECK_COMMITISH "${GIT_COMMIT_OUTPUT}")
            endif()

            # Grab HEAD commit
            execute_process(
                COMMAND ${GIT_EXECUTABLE} rev-parse --verify HEAD
                WORKING_DIRECTORY ${repo_dir}
                OUTPUT_VARIABLE GIT_HEAD_OUTPUT
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )

            if (GIT_HEAD_OUTPUT STREQUAL CHECK_COMMITISH)
                message(STATUS "Repo in ${short_repo_dir} is already at ${PARAM_COMMITISH}. Skipping checkout.")
            else()
                if (GIT_LSREMOTE_RESULT)
                    message(WARNING "No internet connection or remote unavailable. Leaving ${name} as is.")
                else()
                    message(STATUS "Fetching updates for ${name}.")
                    execute_process(
                        COMMAND ${GIT_EXECUTABLE} fetch
                        WORKING_DIRECTORY ${repo_dir}
                        RESULT_VARIABLE GIT_FETCH_RESULT
                    )
                endif()

                message(STATUS "Checking out ${PARAM_COMMITISH} in ${name}.")
                execute_process(
                    COMMAND ${GIT_EXECUTABLE} checkout --quiet ${PARAM_COMMITISH}
                    WORKING_DIRECTORY ${repo_dir}
                    RESULT_VARIABLE GIT_CHECKOUT_RESULT
                )
                if (NOT GIT_CHECKOUT_RESULT)
                    message(STATUS "Checking out ${PARAM_COMMITISH} was successful.")
                else()
                    message(FATAL_ERROR "In ${name}, checking out ${PARAM_COMMITISH} was NOT successful!")
                endif()
            endif()
        else()
            #
            # If rev-parse --verify <PARAM_COMMITISH> failed,
            # we assume it's a branch name that doesn't exist as a direct ref locally
            #
            message(STATUS "COMMITISH ${PARAM_COMMITISH} might be a branch, checking for internet connection.")

            if (GIT_LSREMOTE_RESULT)
                message(WARNING "No internet connection or remote unavailable. Leaving branch ${PARAM_COMMITISH} as-is.")
            else()
                message(STATUS "Fetching updates for branch ${PARAM_COMMITISH}.")
                execute_process(
                    COMMAND ${GIT_EXECUTABLE} fetch
                    WORKING_DIRECTORY ${repo_dir}
                    RESULT_VARIABLE GIT_FETCH_RESULT
                )
                if (NOT GIT_FETCH_RESULT)
                    message(STATUS "Fetch successful.")

                    execute_process(
                        COMMAND ${GIT_EXECUTABLE} branch --show-current
                        WORKING_DIRECTORY ${repo_dir}
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        OUTPUT_VARIABLE GIT_BRANCH_OUTPUT
                        RESULT_VARIABLE GIT_BRANCH_RESULT
                    )
                    if (GIT_BRANCH_RESULT)
                        message(FATAL_ERROR "${repo_dir}: git branch --show-current not successful")
                    endif()

                    if (GIT_BRANCH_OUTPUT STREQUAL "")
                        # Currently detached; let's checkout the branch
                        execute_process(
                            COMMAND ${GIT_EXECUTABLE} checkout --quiet ${PARAM_COMMITISH}
                            WORKING_DIRECTORY ${repo_dir}
                            RESULT_VARIABLE GIT_CHECKOUT_RESULT
                        )
                        if (NOT GIT_CHECKOUT_RESULT)
                            message(STATUS "In ${name}, checking out branch ${PARAM_COMMITISH} was successful.")
                        else()
                            message(FATAL_ERROR "In ${name}, checking out branch ${PARAM_COMMITISH} was NOT successful!")
                        endif()
                    else()
                        message(WARNING
                            "${short_repo_dir} is on branch ${GIT_BRANCH_OUTPUT}, leaving it there. "
                            "NOT checking out ${PARAM_COMMITISH}! Use origin/main or similar if you "
                            "want to stay up-to-date with upstream."
                        )
                    endif()
                else()
                    message(WARNING "Fetching ${name} was NOT successful!")
                endif()
            endif()
        endif()
    else()
        # If the repo doesn't exist, do a fresh clone
        message(STATUS "Cloning ${PARAM_URL} to ${short_repo_dir}.")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --recurse-submodules ${PARAM_URL} ${repo_dir}
            RESULT_VARIABLE GIT_CLONE_RESULT
        )
        if (NOT GIT_CLONE_RESULT)
            execute_process(
                COMMAND ${GIT_EXECUTABLE} checkout --quiet ${PARAM_COMMITISH}
                WORKING_DIRECTORY ${repo_dir}
                RESULT_VARIABLE GIT_CHECKOUT_RESULT
            )
            if (NOT GIT_CHECKOUT_RESULT)
                message(STATUS "Checking out ${PARAM_COMMITISH} was successful.")
            else()
                message(FATAL_ERROR "In ${name}, checking out ${PARAM_COMMITISH} was NOT successful!")
            endif()
        else()
            message(FATAL_ERROR "Cloning ${name} was NOT successful!")
        endif()
    endif()

    if (NOT ${PARAM_DO_NOT_ADD_SUBPROJECT})
        if (NOT ${PARAM_NOT_SYSTEM})
            add_subdirectory(${repo_dir} ${CMAKE_BINARY_DIR}/alp_external/${name} SYSTEM)
        else()
            add_subdirectory(${repo_dir} ${CMAKE_BINARY_DIR}/alp_external/${name})
        endif()
    endif()
endfunction()
