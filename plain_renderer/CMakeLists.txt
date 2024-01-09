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

project(alpine-renderer-plain_renderer LANGUAGES CXX)

qt_add_executable(plain_renderer
    main.cpp
    Window.h Window.cpp
)
set_target_properties(plain_renderer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
target_link_libraries(plain_renderer PUBLIC gl_engine)
target_include_directories(plain_renderer PRIVATE .)

qt_add_resources(plain_renderer "height_data"
    PREFIX "/map"
    BASE ${renderer_static_data_SOURCE_DIR}
    FILES ${renderer_static_data_SOURCE_DIR}/height_data.atb
)

if (EMSCRIPTEN)
    message(NOTICE "ALP_WWW_INSTALL_DIR = ${ALP_WWW_INSTALL_DIR}")
    configure_file(../site/mascot.png mascot.png COPYONLY)
    set(ALP_INSTALL_FILES
        "$<TARGET_FILE_DIR:plain_renderer>/plain_renderer.js"
        "$<TARGET_FILE_DIR:plain_renderer>/plain_renderer.wasm"
        "$<TARGET_FILE_DIR:plain_renderer>/plain_renderer.html"
        "$<TARGET_FILE_DIR:plain_renderer>/qtloader.js"
        "${CMAKE_SOURCE_DIR}/site/mascot.png"
    )
    if (ALP_ENABLE_THREADING)
        list(APPEND ALP_INSTALL_FILES "$<TARGET_FILE_DIR:plain_renderer>/plain_renderer.worker.js")
    endif()
    install(FILES ${ALP_INSTALL_FILES} DESTINATION ${ALP_WWW_INSTALL_DIR})

endif()
