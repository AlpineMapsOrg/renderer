#############################################################################
# AlpineMaps.org
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

if(NOT DEFINED _alp_add_repo_check_flag)
    set_property(GLOBAL PROPERTY _alp_add_repo_check_flag FALSE)
endif()

function(_alp_git_checkout repo repo_dir commitish)
    message(STATUS "[alp/git] ${repo}: Checking out ${commitish}.")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} checkout --quiet ${commitish}
        WORKING_DIRECTORY ${repo_dir}
        RESULT_VARIABLE GIT_CHECKOUT_RESULT
        ERROR_VARIABLE checkout_output
        OUTPUT_VARIABLE checkout_output
    )
    if (NOT GIT_CHECKOUT_RESULT)
        # message(STATUS "[alp/git] In ${repo}, checking out ${commitish} succeeded.")
    else()
        message(WARNING "[alp/git] ${repo}: Checking out ${commitish} was NOT successful: ${checkout_output}")
    endif()

    if (EXISTS "${repo_dir}/.gitmodules")
        # init/update submodules; for shallow clones this will still be shallow because the super‑project is.
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive ${_ALP_SUBMODULE_UPDATE_ARGS}
            WORKING_DIRECTORY ${repo_dir}
            RESULT_VARIABLE GIT_SUBMODULE_RESULT
            ERROR_VARIABLE checkout_output
            OUTPUT_VARIABLE checkout_output
        )
        if(GIT_SUBMODULE_RESULT EQUAL 0)
            # message(STATUS "[alp/git] ${repo}: Submodules updated to match ${commitish}.")
        else()
            message(WARNING "[alp/git] ${repo}: Submodule update failed after checking out ${commitish}: ${checkout_output}")
        endif()
    endif()
endfunction()

function(alp_add_git_repository name)
    set(options DO_NOT_ADD_SUBPROJECT NOT_SYSTEM DEEP_CLONE PRIVATE_DO_NOT_CHECK_FOR_SCRIPT_UPDATES)
    set(oneValueArgs URL COMMITISH DESTINATION_PATH)
    set(multiValueArgs )
    cmake_parse_arguments(PARSE_ARGV 1 PARAM "${options}" "${oneValueArgs}" "${multiValueArgs}")

    # Determine whether we should do a shallow or deep clone/fetch.
    if(PARAM_DEEP_CLONE)
        # message(STATUS "[alp/git] Cloning ${PARAM_URL} DEEPLY to ${repo_dir}.")
        set(_ALP_GIT_CLONE_ARGS)
        set(_ALP_SUBMODULE_UPDATE_ARGS)
    else()
        # message(STATUS "[alp/git] Cloning ${PARAM_URL} SHALLOWY to ${repo_dir}.")
        set(_ALP_GIT_CLONE_ARGS --no-checkout --depth 1 --shallow-submodules)
        set(_ALP_SUBMODULE_UPDATE_ARGS --depth 1 --recommend-shallow)
    endif()

    get_property(_check_ran GLOBAL PROPERTY _alp_add_repo_check_flag)
    if(NOT PARAM_PRIVATE_DO_NOT_CHECK_FOR_SCRIPT_UPDATES AND NOT _check_ran)
        if(NOT COMMAND alp_check_for_script_updates)
            include(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CheckForScriptUpdates.cmake)
        endif()
        alp_check_for_script_updates(${CMAKE_CURRENT_FUNCTION_LIST_FILE})
        set_property(GLOBAL PROPERTY _alp_add_repo_check_flag TRUE)
    endif()

    if (NOT DEFINED ALP_EXTERN_DIR OR ALP_EXTERN_DIR STREQUAL "")
        set(ALP_EXTERN_DIR extern)
    endif()

    set(repo_dir ${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/${name})
    set(short_repo_dir ${ALP_EXTERN_DIR}/${name})
    if (DEFINED PARAM_DESTINATION_PATH AND NOT PARAM_DESTINATION_PATH STREQUAL "")
        set(repo_dir ${CMAKE_SOURCE_DIR}/${PARAM_DESTINATION_PATH})
        set(short_repo_dir ${PARAM_DESTINATION_PATH})
    endif()
    file(MAKE_DIRECTORY ${repo_dir})

    set(${name}_SOURCE_DIR "${repo_dir}" PARENT_SCOPE)

    # Detect if the requested ref looks like a remote branch (e.g. origin/main)
    string(REGEX MATCH "^[^/]+/.+" force_fetch "${PARAM_COMMITISH}")

    set(force_checkout FALSE)
    if(NOT EXISTS "${repo_dir}/.git")
        # Do a fresh clone
        message(STATUS "[alp/git] ${short_repo_dir}: Cloning ${PARAM_URL} to ${repo_dir}.")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone ${_ALP_GIT_CLONE_ARGS} --recurse-submodules ${PARAM_URL} ${repo_dir}
            RESULT_VARIABLE GIT_CLONE_RESULT
            ERROR_VARIABLE clone_output
            OUTPUT_VARIABLE clone_output
        )
        if (NOT GIT_CLONE_RESULT EQUAL 0)
            message(SEND_ERROR "[alp/git] ${short_repo_dir}: Cloning was NOT successful: ${clone_output}")
        endif()
        if (NOT PARAM_DEEP_CLONE)
            # shallow clone, fetch ref and checkout
            set(force_fetch TRUE)
        endif()
        set(force_checkout TRUE)
    endif()

    # Check out the correct branch:
    # First, see if PARAM_COMMITISH is a valid local ref:
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --verify ${PARAM_COMMITISH}
        WORKING_DIRECTORY ${repo_dir}
        OUTPUT_VARIABLE GIT_COMMIT_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE commit_present_result
        ERROR_QUIET
    )
    if (commit_present_result EQUAL 0 AND NOT force_fetch)
        # PARAM_COMMITISH is recognized by Git => no need to fetch
        # (could be a tag (lightweight or annotated) or a direct commit SHA).
        # if it's an *annotated* tag, rev-parse gives us the tag object's hash, not the commit hash.
        # => Force resolve the actual commit object with ^{commit}:
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --verify ${PARAM_COMMITISH}^{commit}
            WORKING_DIRECTORY ${repo_dir}
            OUTPUT_VARIABLE GIT_COMMIT_OBJECT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE commit_object_result
        )

        if (commit_object_result EQUAL 0)
            # Successfully resolved a commit object
            set(commitish_hash "${GIT_COMMIT_OBJECT}")
        else()
            # Fallback if that fails (should rarely happen if it's a proper commit/tag)
            set(commitish_hash "${GIT_COMMIT_OUTPUT}")
        endif()

        # Grab HEAD commit
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --verify HEAD
            WORKING_DIRECTORY ${repo_dir}
            OUTPUT_VARIABLE git_head_hash
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if (git_head_hash STREQUAL commitish_hash AND NOT force_checkout)
            message(STATUS "[alp/git] ${short_repo_dir}: Already at ${PARAM_COMMITISH}. Skipping checkout.")
        else()
            _alp_git_checkout(${short_repo_dir} ${repo_dir} ${PARAM_COMMITISH})
        endif()
    else()
        # either remote branch or commitish not recognised
        message(STATUS "[alp/git] ${short_repo_dir}: Fetching updates.")

        if(PARAM_DEEP_CLONE)
            # Original deep‑clone logic: fetch everything (incl. all tags) deeply.
            # message(STATUS "[alp/git] Deep clone, fetching all.")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} fetch origin --tags
                WORKING_DIRECTORY ${repo_dir}
                RESULT_VARIABLE fetch_result
                OUTPUT_QUIET
                ERROR_QUIET
            )
        else()
            # ── Shallow path ─────────────────────────
            # 1. Try to fetch COMMITISH as a tag
            # message(STATUS "[alp/git] Shallow clone, trying to fetch tag ${PARAM_COMMITISH}.")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} fetch origin tag ${PARAM_COMMITISH} --depth 1
                WORKING_DIRECTORY ${repo_dir}
                RESULT_VARIABLE fetch_result
                OUTPUT_QUIET
                ERROR_QUIET
            )

            # 2. If that failed, try it as branch‑name or raw hash
            if(NOT fetch_result EQUAL 0)
                # message(STATUS "[alp/git] Shallow clone, failed to fetch tag ${PARAM_COMMITISH}. Probably it is a branch or hash. Trying again..")
                set(_FETCH_REF "${PARAM_COMMITISH}")
                string(REGEX REPLACE "^origin/(.+)" "\\1" _FETCH_REF "${_FETCH_REF}")
                execute_process(
                    COMMAND ${GIT_EXECUTABLE} fetch origin ${_FETCH_REF} --depth 1
                    WORKING_DIRECTORY ${repo_dir}
                    RESULT_VARIABLE fetch_result
                    OUTPUT_QUIET
                    ERROR_QUIET
                )
            endif()
        endif()

        if (fetch_result EQUAL 0)
            # message(STATUS "[alp/git] Fetch successful for ${short_repo_dir}.")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} branch --show-current
                WORKING_DIRECTORY ${repo_dir}
                OUTPUT_STRIP_TRAILING_WHITESPACE
                OUTPUT_VARIABLE current_branch
                RESULT_VARIABLE branch_result
            )
            if (NOT branch_result EQUAL 0)
                message(FATAL_ERROR "[alp/git] ${short_repo_dir}: git branch --show-current failed")
            endif()

            if (current_branch STREQUAL "" OR force_checkout)
                # not on a branch. that's what we usually have (detached head state)
                _alp_git_checkout(${short_repo_dir} ${repo_dir} ${PARAM_COMMITISH})
            else()
                message(WARNING "[alp/git] ${short_repo_dir}: On branch ${current_branch}, leaving it there. NOT checking out ${PARAM_COMMITISH}! Use origin/main or similar if you want to stay up‑to‑date with upstream.")
            endif()
        else()
            message(WARNING "[alp/git] ${short_repo_dir}: Unable to fetch updates for; ${PARAM_COMMITISH} not found locally or is a remote branch.")
        endif()
    endif()

    if (NOT PARAM_DO_NOT_ADD_SUBPROJECT)
        if (NOT PARAM_NOT_SYSTEM)
            add_subdirectory(${repo_dir} ${CMAKE_BINARY_DIR}/alp_external/${name} SYSTEM)
        else()
            add_subdirectory(${repo_dir} ${CMAKE_BINARY_DIR}/alp_external/${name})
        endif()
    endif()
endfunction()
