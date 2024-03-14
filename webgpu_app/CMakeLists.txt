#############################################################################
# weBIGeo
# Copyright (C) 2024 Adam Celarek <family name at cg tuwien ac at>
# Copyright (C) 2024 Gerald Kimmersdorfer
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

project(alpine-renderer-webgpu_app LANGUAGES C CXX)

# option to enable/disable imgui
option(ALP_WEBGPU_APP_ENABLE_IMGUI "Enable imgui" ON)


set(SOURCES
    main.cpp
    TerrainRenderer.h TerrainRenderer.cpp
)

if (EMSCRIPTEN)
    list(APPEND SOURCES WebInterop.h WebInterop.cpp)
endif()

if (ALP_WEBGPU_APP_ENABLE_IMGUI)
    list(APPEND SOURCES imgui_style.h)
endif()

qt_add_executable(webgpu_app
    resources.qrc
    ${SOURCES}
)


set_target_properties(webgpu_app PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# TODO: Get rid of Qt::GUI (no QImage)
target_link_libraries(webgpu_app PUBLIC webgpu webgpu_engine Qt::Core Qt::Gui)
target_include_directories(webgpu_app PRIVATE .)

if (EMSCRIPTEN)
    target_link_options(webgpu_app PRIVATE
            -sUSE_GLFW=3 # Use Emscripten-provided GLFW
            -sUSE_WEBGPU # Handle WebGPU symbols
            -sASYNCIFY # Required by WebGPU-C++
            #-sEXPORTED_FUNCTIONS=_main,_set_glfw_window_size # _main needs to be specifically exported if sEXPORTED_FUNCTIONS is set
            #-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF16ToString,stringToUTF16,JSEvents,specialHTMLTargets,FS # will be overwritten by Qt
            #--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/resources # resources pls via qt resource system
            #--shell-file "${CMAKE_CURRENT_SOURCE_DIR}/shell/${SHELL_FILE}" # setting custom shell here makes no sense as qt will overwrite it anyway
            -msimd128   #enables auto-vectorization and simd support
            -lembind #enable embind
            #-gsource-map #generate source maps

            # FOLLOWING IS SET BY Qt6.6.1 (Single thread)
            #-s INITIAL_MEMORY=50MB
            #-s EXPORTED_RUNTIME_METHODS=UTF16ToString,stringToUTF16,JSEvents,specialHTMLTargets,FS
            #-s MAX_WEBGL_VERSION=2
            #-s FETCH=1  # necessary for Network-Requests
            #-s WASM_BIGINT=1
            #-s STACK_SIZE=5MB
            #-s MODULARIZE=1
            #-s EXPORT_NAME=createQtAppInstance
            #-s DISABLE_EXCEPTION_CATCHING=1
            #-s ALLOW_MEMORY_GROWTH
            #-s DEMANGLE_SUPPORT=1
            #--profiling-funcs
            #-sASYNCIFY_IMPORTS=qt_asyncify_suspend_js,qt_asyncify_resume_js
            #-s ERROR_ON_UNDEFINED_SYMBOLS=1
    )

    # Copy custom shell (name of file == name of target)
    # IMPORTANT: THIS WILL ONLY BE EXECUTED WHEN THE TARGET IS ACTUALLY REBUILT
    # SO THERE HAVE TO BE CHANGES IN THE C++ CODE. THIS IS PARTICULARLY IMPORTANT
    # TO KNOW WHEN YOU WORK ON THE CUSTOM SHELL
    add_custom_command(TARGET webgpu_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/shell/fullscreen_qt_webgpu_shell.html
                ${CMAKE_CURRENT_BINARY_DIR}/webgpu_app.html
    )


endif()

qt_add_resources(webgpu_app "height_data"
    PREFIX "/map"
    BASE ${renderer_static_data_SOURCE_DIR}
    FILES ${renderer_static_data_SOURCE_DIR}/height_data.atb
)
