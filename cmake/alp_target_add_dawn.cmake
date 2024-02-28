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

# alp_target_add_dawn(target, dawn_path, scope)
# Configures a target to link against and include Dawn graphics libraries.
# - target: Target to configure.
# - dawn_path: Path to Dawn installation. (Builds are expected to be in build/debug or build/release)
# - scope: Linkage and include scope (PRIVATE, PUBLIC, INTERFACE).
function(alp_target_add_dawn target dawn_path scope)

    set(DAWN_DIR "${dawn_path}")

    # Determine the correct DAWN_BIN based on the build type
    set(DAWN_BIN "${DAWN_DIR}/build/release")
    if(CMAKE_BUILD_TYPE MATCHES Debug)
        set(DAWN_BIN "${DAWN_DIR}/build/debug")
    endif()

    # Check if DAWN_DIR exists
    if(NOT EXISTS "${DAWN_DIR}")
        message(FATAL_ERROR "DAWN could not be found at: ${DAWN_DIR}")
    endif()

    # Check if DAWN_BIN exists
    if(NOT EXISTS "${DAWN_BIN}")
        message(FATAL_ERROR "DAWN Binaries could not be found at: ${DAWN_BIN}. Did you build DAWN?")
    endif()

    # Note: Dawn constitutes of multiple libraries, so you have to link all of them
    # in order to not end up with unresolved symbols. A subset of the libraries might
    # be sufficient, but finding this is not trivial, and may change with future versions.
    # Therefore, linking all libraries might be the easiest/best approach

    # Find all .lib files in the Dawn build directory
    file(GLOB_RECURSE DAWN_LIBRARIES "${DAWN_BIN}/*.lib")

    # Link all found libraries to the specified target
    target_link_libraries(${target} ${scope} ${DAWN_LIBRARIES})

    # Add the header files as well as the generated headers to the include directories
    target_include_directories(${target} ${scope} "${DAWN_DIR}/include")
    target_include_directories(${target} ${scope} "${DAWN_BIN}/gen/include")
endfunction(alp_target_add_dawn)
