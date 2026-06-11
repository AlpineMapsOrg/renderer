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


set(SOURCES
    main.cpp
    TerrainRenderer.h TerrainRenderer.cpp
    RenderingContext.h RenderingContext.cpp
    InputMapper.h InputMapper.cpp
    CloudsManager.h CloudsManager.cpp

    util/error_logging.h util/error_logging.cpp

    util/dark_mode.h util/dark_mode.cpp

    ImGuiManager.h ImGuiManager.cpp
)

list(APPEND SOURCES
        imgui/ImGuiPanel.h
        imgui/TimingPanel.h imgui/TimingPanel.cpp
        imgui/CameraPanel.h imgui/CameraPanel.cpp
        imgui/AppPanel.h imgui/AppPanel.cpp
        imgui/CloudPanel.h imgui/CloudPanel.cpp
        imgui/AtmospherePanel.h imgui/AtmospherePanel.cpp
        imgui/ShadingPanel.h imgui/ShadingPanel.cpp
        imgui/AboutPanel.h imgui/AboutPanel.cpp
        imgui/CompassPanel.h imgui/CompassPanel.cpp
        imgui/LogoPanel.h imgui/LogoPanel.cpp
        imgui/SearchPanel.h imgui/SearchPanel.cpp
        imgui/OverlaysPanel.h imgui/OverlaysPanel.cpp
        imgui/TrackPanel.h imgui/TrackPanel.cpp
        imgui/overlays/OverlayImGuiRenderer.h imgui/overlays/OverlayImGuiRenderer.cpp
        imgui/overlays/OverlayImGuiRendererFactory.h imgui/overlays/OverlayImGuiRendererFactory.cpp
        imgui/overlays/HeightLinesOverlayImGuiRenderer.h imgui/overlays/HeightLinesOverlayImGuiRenderer.cpp
        imgui/overlays/ScreenSpaceSnowOverlayImGuiRenderer.h imgui/overlays/ScreenSpaceSnowOverlayImGuiRenderer.cpp
        imgui/overlays/TextureOverlayImGuiRenderer.h imgui/overlays/TextureOverlayImGuiRenderer.cpp
        imgui/overlays/TileDebugOverlayImGuiRenderer.h imgui/overlays/TileDebugOverlayImGuiRenderer.cpp
        SearchService.h SearchService.cpp
    )

if (ALP_WEBGPU_APP_ENABLE_COMPUTE)
    list(APPEND SOURCES
        compute/OverlayRenderNode.h compute/OverlayRenderNode.cpp
        imgui/NodeGraphPanel.h imgui/NodeGraphPanel.cpp
        imgui/nodes/NodeRenderer.h imgui/nodes/NodeRenderer.cpp
        imgui/nodes/NodeRendererFactory.h imgui/nodes/NodeRendererFactory.cpp
        imgui/nodes/ExportNodeRenderer.h imgui/nodes/ExportNodeRenderer.cpp
        imgui/nodes/OverlayNodeRenderer.h imgui/nodes/OverlayNodeRenderer.cpp
        imgui/nodes/BufferToTextureNodeRenderer.h imgui/nodes/BufferToTextureNodeRenderer.cpp
        imgui/nodes/ComputeAvalancheTrajectoriesNodeRenderer.h imgui/nodes/ComputeAvalancheTrajectoriesNodeRenderer.cpp
        imgui/nodes/ComputeReleasePointsNodeRenderer.h imgui/nodes/ComputeReleasePointsNodeRenderer.cpp
        imgui/nodes/ComputeSnowNodeRenderer.h imgui/nodes/ComputeSnowNodeRenderer.cpp
        imgui/nodes/RequestTilesNodeRenderer.h imgui/nodes/RequestTilesNodeRenderer.cpp
        imgui/nodes/SelectTilesNodeRenderer.h imgui/nodes/SelectTilesNodeRenderer.cpp
        imgui/nodes/GPXTrackNodeRenderer.h imgui/nodes/GPXTrackNodeRenderer.cpp
    )
endif()

if (EMSCRIPTEN)
    list(APPEND SOURCES WebInterop.h WebInterop.cpp)
endif()

qt_add_executable(webgpu_app
    resources.qrc
    ${SOURCES}
)


set_target_properties(webgpu_app PROPERTIES
    WIN32_EXECUTABLE FALSE  # Set to FALSE to keep console attached on Windows
    MACOSX_BUNDLE TRUE
)

target_link_libraries(webgpu_app PUBLIC webgpu webgpu_engine Qt::Core Qt::Network)
target_include_directories(webgpu_app PRIVATE .)

if (ALP_WEBGPU_APP_ENABLE_COMPUTE)
    target_link_libraries(webgpu_app PUBLIC webgpu_compute)
    target_compile_definitions(webgpu_app PRIVATE ALP_WEBGPU_APP_ENABLE_COMPUTE)
endif()

# For Font Awesome Icon Headers:
    alp_add_git_repository(iconfontcppheaders URL https://github.com/juliettef/IconFontCppHeaders.git COMMITISH f30b1e73b2d71eb331d77619c3f1de34199afc38 DO_NOT_ADD_SUBPROJECT)
    target_include_directories(webgpu_app PRIVATE ../extern/iconfontcppheaders)

    alp_add_git_repository(imgui URL https://github.com/AlpineMapsOrgDependencies/imgui_slim.git COMMITISH 4d8ecbf58ebf32e757ff56a60f1fb0446033edb8)

    set(IMNODES_IMGUI_TARGET_NAME imgui)
    set(BUILD_SHARED_LIBS_SAVED ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    alp_add_git_repository(imnodes URL https://github.com/AlpineMapsOrgDependencies/imnodes_slim.git COMMITISH 324355e64ed8b5bea02fe1439cdcb5c7773b4436)
    set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_SAVED})

    alp_add_git_repository(ImGuiFileDialog URL https://github.com/AlpineMapsOrgDependencies/ImGuiFileDialog_slim.git COMMITISH 39f98eb131ee00d476f1002336fc7e9e1795ccc5)
    target_link_libraries(ImGuiFileDialog PRIVATE imgui)

    target_link_libraries(webgpu_app PRIVATE imgui imnodes ImGuiFileDialog)

# Copy necessary DLLs to the output directory on Windows
if (WIN32 AND NOT EMSCRIPTEN)
    # Copy SDL2 DLL
    if (EXISTS "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/sdl/bin/SDL2.dll")
        add_custom_command(TARGET webgpu_app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/sdl/bin/SDL2.dll"
            "$<TARGET_FILE_DIR:webgpu_app>"
            COMMENT "Copying SDL2.dll"
        )
    endif()

    # Use Qt's windeployqt to copy all necessary Qt dependencies
    get_target_property(QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET webgpu_app POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations "$<TARGET_FILE:webgpu_app>"
            COMMENT "Running windeployqt for webgpu_app"
        )
    endif()


    # === INSTALL CONFIGURATION FOR NATIVE WINDOWS BUILD ===
    install(TARGETS webgpu_app RUNTIME DESTINATION . BUNDLE DESTINATION .)
    if (EXISTS "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/sdl/bin/SDL2.dll")
        install(FILES "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/sdl/bin/SDL2.dll" DESTINATION .)
    endif()
    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            execute_process(
                COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-translations \"\${CMAKE_INSTALL_PREFIX}/webgpu_app.exe\"
            )
        ")
    endif()
    # ================================================
endif()

if (EMSCRIPTEN)

    target_link_options(webgpu_app PRIVATE
            --use-port=sdl2 # Use emscripten SDL2 implementation
            #--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/resources # DONT USE RESOURCES PLS VIA QT RESOURCE SYSTEM
            #--shell-file "${CMAKE_CURRENT_SOURCE_DIR}/shell/${SHELL_FILE}" # DOESNT WORK AS QT WILL OVERWRITE IT
            -msimd128   #enables auto-vectorization and simd support
            -lembind #enable embind
            #-gsource-map #generate source maps

            # FOLLOWING IS SET BY Qt6.10
            #-s EXPORTED_RUNTIME_METHODS=UTF16ToString,stringToUTF16,JSEvents,specialHTMLTargets,FS,callMain
            #-s EXPORTED_FUNCTIONS=_main,__embind_initialize_bindings
            #-s PTHREAD_POOL_SIZE=4
            #-s INITIAL_MEMORY=50MB
            #-s MAXIMUM_MEMORY=4GB
            #-s MAX_WEBGL_VERSION=2
            #-s WASM_BIGINT=1
            #-s STACK_SIZE=5MB
            #-pthread
            #-s ALLOW_MEMORY_GROWTH
            #--profiling-funcs
            #-sERROR_ON_UNDEFINED_SYMBOLS=1
            #-sFETCH
            #-sASYNCIFY=1
    )

    # enables to append to EXPORTED_RUNTIME_METHODS which is overridden by Qt
    # see issue (https://bugreports.qt.io/browse/QTBUG-104882) and fix (https://codereview.qt-project.org/c/qt/qtbase/+/421733)
    set_target_properties(webgpu_app PROPERTIES QT_WASM_EXTRA_EXPORTED_METHODS "ccall,cwrap")

    # Copy custom shell (name of file == name of target)
    # IMPORTANT: THIS WILL ONLY BE EXECUTED WHEN THE TARGET IS ACTUALLY REBUILT
    # SO THERE HAVE TO BE CHANGES IN THE C++ CODE.
    file(GLOB_RECURSE SHELL_FILES "${CMAKE_CURRENT_SOURCE_DIR}/shell/*")
    message(STATUS "SHELL_FILES: ${SHELL_FILES}")
    # now create a custom command to move all those files to the build directory
    foreach(SHELL_FILE ${SHELL_FILES})
        get_filename_component(SHELL_FILE_NAME ${SHELL_FILE} NAME)
        add_custom_command(TARGET webgpu_app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove -f
            ${CMAKE_CURRENT_BINARY_DIR}/${SHELL_FILE_NAME}
            COMMAND ${CMAKE_COMMAND} -E copy
            ${SHELL_FILE}
            ${CMAKE_CURRENT_BINARY_DIR}/${SHELL_FILE_NAME}
        )
    endforeach()

    # === INSTALL CONFIGURATION FOR WASM BUILD ===
    set(WASM_OUTPUT_FILES
        ${CMAKE_CURRENT_BINARY_DIR}/webgpu_app.html
        ${CMAKE_CURRENT_BINARY_DIR}/webgpu_app.js
        ${CMAKE_CURRENT_BINARY_DIR}/webgpu_app.wasm
        ${CMAKE_CURRENT_BINARY_DIR}/qtloader.js
    )
    foreach(SHELL_FILE ${SHELL_FILES})
        get_filename_component(SHELL_FILE_NAME ${SHELL_FILE} NAME)
        list(APPEND WASM_OUTPUT_FILES ${CMAKE_CURRENT_BINARY_DIR}/${SHELL_FILE_NAME})
    endforeach()
    install(FILES ${WASM_OUTPUT_FILES} DESTINATION .)
    install(CODE "file(RENAME \"\${CMAKE_INSTALL_PREFIX}/webgpu_app.html\" \"\${CMAKE_INSTALL_PREFIX}/index.html\")")
    # ================================================
endif()
