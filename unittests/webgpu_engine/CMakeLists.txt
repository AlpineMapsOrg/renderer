#############################################################################
# Alpine Terrain Renderer
# Copyright (C) 2023 Adam Celarek <family name at cg tuwien ac at>
# Copyright (C) 2023 Gerald Kimmersdorfer
# Copyright (C) 2024 Patrick Komon
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

project(alpine-renderer-unittests_webgpu_engine LANGUAGES CXX)

alp_add_unittest(unittests_webgpu_engine
    UnittestWebgpuContext.h UnittestWebgpuContext.cpp
    test_GpuShaderFunctions.cpp
    test_ShaderPreprocessor.cpp
)

target_link_libraries(unittests_webgpu_engine PUBLIC webgpu_engine)

# Copy necessary DLLs to the output directory on Windows
if (WIN32 AND NOT EMSCRIPTEN)
    # Copy Qt DLLs
    add_custom_command(TARGET unittests_webgpu_engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE_DIR:unittests_webgpu_engine>"
        COMMENT "Copying Qt6Core DLL to unittests"
    )

    # Copy SDL2 DLL
    if (EXISTS "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/sdl/bin/SDL2.dll")
        add_custom_command(TARGET unittests_webgpu_engine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/sdl/bin/SDL2.dll"
            "$<TARGET_FILE_DIR:unittests_webgpu_engine>"
            COMMENT "Copying SDL2.dll to unittests"
        )
    endif()
endif()

