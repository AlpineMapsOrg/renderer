#############################################################################
# Alpine Terrain Renderer
# Copyright (C) 2023 Adam Celarek <family name at cg tuwien ac at>
# Copyright (C) 2023 Gerald Kimmersdorfer
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

cmake_minimum_required(VERSION 3.24)
project(alpine-renderer-unittests_gl_engine LANGUAGES CXX)

qt_add_executable(unittests_gl_engine 
    UnittestGlWindow.h UnittestGlWindow.cpp
    main.cpp
    framebuffer.cpp
    uniformbuffer.cpp
)

target_link_libraries(unittests_gl_engine PUBLIC gl_engine Catch2::Catch2)

alp_setup_unittest(unittests_gl_engine)

if (EMSCRIPTEN)
    configure_file(${CMAKE_SOURCE_DIR}/site/unittests_gl.html unittests_gl.html COPYONLY)
    configure_file(${CMAKE_SOURCE_DIR}/site/ansispan.js ansispan.js COPYONLY)

    install(FILES
        "$<TARGET_FILE_DIR:unittests_gl_engine>/unittests_gl_engine.js"
        "$<TARGET_FILE_DIR:unittests_gl_engine>/unittests_gl_engine.wasm"
        "$<TARGET_FILE_DIR:unittests_gl_engine>/qtloader.js"
        "${CMAKE_SOURCE_DIR}/site/unittests_gl.html"
        "${CMAKE_SOURCE_DIR}/site/ansispan.js"
        "${CMAKE_SOURCE_DIR}/site/mascot.png"
        DESTINATION ${ALP_WWW_INSTALL_DIR})

    if (ALP_ENABLE_THREADING)
        install(FILES "$<TARGET_FILE_DIR:unittests_gl_engine>/unittests_gl_engine.worker.js" DESTINATION ${ALP_WWW_INSTALL_DIR})
    endif()
endif()
