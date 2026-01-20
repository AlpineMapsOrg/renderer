#############################################################################
# weBIGeo
# Copyright (C) 2025 Adam Celarek <family name at cg tuwien ac at>
# Copyright (C) 2025 Gerald Kimmersdorfer
# Copyright (C) 2025 Patrick Komon
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

project(alpine-renderer-webigeo_eval LANGUAGES C CXX)

include(GenerateBatchScript.cmake)

set(SOURCES
    main.cpp
    WebigeoApp.h WebigeoApp.cpp
    Settings.h Settings.cpp

    util/error_logging.h util/error_logging.cpp
    util/webgpu_init.h util/webgpu_init.cpp
)

qt_add_executable(webigeo_eval
    ${SOURCES}
)

set_target_properties(webigeo_eval PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(webigeo_eval PRIVATE webgpu webgpu_engine Qt::Core)
target_include_directories(webigeo_eval PRIVATE .)

generate_webigeo_eval_batch_script()
