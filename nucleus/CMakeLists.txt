#############################################################################
# Alpine Terrain Renderer
# Copyright (C) 2023 Adam Celarek <family name at cg tuwien ac at>
# Copyright (C) 2024 Lucas Dworschak
# Copyright (C) 2023 Gerald Kimmersdorfer
# Copyright (C) 2015 Taylor Braun-Jones (via github.com/nocnokneo/cmake-git-versioning-example)
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

project(alpine-renderer-nucleus LANGUAGES CXX)

alp_add_git_repository(stb_slim URL https://github.com/AlpineMapsOrgDependencies/stb_slim.git COMMITISH c44329cf0aae422c5c144a043e2ca47e9d9cc204)
alp_add_git_repository(radix URL https://github.com/AlpineMapsOrg/radix.git COMMITISH v24.01.20 NOT_SYSTEM)
alp_add_git_repository(tl_expected URL https://github.com/TartanLlama/expected.git COMMITISH v1.1.0 DO_NOT_ADD_SUBPROJECT)
if (NOT TARGET fmt)
    alp_add_git_repository(fmt URL https://github.com/fmtlib/fmt.git COMMITISH 10.1.1)
endif()
alp_add_git_repository(zppbits URL https://github.com/eyalz800/zpp_bits.git COMMITISH v4.4.20 DO_NOT_ADD_SUBPROJECT)

add_library(zppbits INTERFACE)
target_include_directories(zppbits SYSTEM INTERFACE ${zppbits_SOURCE_DIR})

add_library(tl_expected INTERFACE)
target_include_directories(tl_expected INTERFACE ${tl_expected_SOURCE_DIR}/include)

set(alp_version_out ${CMAKE_BINARY_DIR}/alp_version/nucleus/version.cpp)

# cmake tests for existance of ${alp_version_out}.do_always_run. since it's always missing, cmake tries to generate it using this command.
# this makes sure, that the command is always run. ${alp_version_out} is not always updated, so nucleus_version is only recompiled
# if the version really changes.
add_custom_command(
    OUTPUT ${alp_version_out} ${alp_version_out}.do_always_run
    COMMAND ${CMAKE_COMMAND} -D ALP_VERSION_TEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/version.cpp.in
                             -D ALP_VERSION_DESTINATION=${alp_version_out}
                             -P ${CMAKE_SOURCE_DIR}/cmake/alp_generate_version_file.cmake
    COMMENT "Updating ${alp_version_out}"
)

add_library(nucleus_version STATIC
    version.h
    ${alp_version_out}
)

qt_add_library(nucleus STATIC
    AbstractRenderWindow.h AbstractRenderWindow.cpp
    Controller.h Controller.cpp
    event_parameter.h
    Raster.h
    srs.h srs.cpp
    Tile.cpp Tile.h
    tile_scheduler/utils.h tile_scheduler/utils.cpp
    tile_scheduler/DrawListGenerator.h tile_scheduler/DrawListGenerator.cpp
    tile_scheduler/LayerAssembler.h tile_scheduler/LayerAssembler.cpp
    tile_scheduler/tile_types.h
    tile_scheduler/constants.h
    tile_scheduler/QuadAssembler.h tile_scheduler/QuadAssembler.cpp
    tile_scheduler/Cache.h
    tile_scheduler/TileLoadService.h tile_scheduler/TileLoadService.cpp
    tile_scheduler/Scheduler.h tile_scheduler/Scheduler.cpp
    tile_scheduler/SlotLimiter.h tile_scheduler/SlotLimiter.cpp
    tile_scheduler/RateLimiter.h tile_scheduler/RateLimiter.cpp
    camera/CadInteraction.h camera/CadInteraction.cpp
    camera/Controller.h camera/Controller.cpp
    camera/Definition.h camera/Definition.cpp
    camera/FirstPersonInteraction.h camera/FirstPersonInteraction.cpp
    camera/InteractionStyle.h camera/InteractionStyle.cpp
    camera/OrbitInteraction.h camera/OrbitInteraction.cpp
    camera/RotateNorthAnimation.h camera/RotateNorthAnimation.cpp
    camera/AbstractDepthTester.h
    camera/PositionStorage.h camera/PositionStorage.cpp
    utils/Stopwatch.h utils/Stopwatch.cpp
    utils/terrain_mesh_index_generator.h
    utils/tile_conversion.h utils/tile_conversion.cpp
    utils/UrlModifier.h utils/UrlModifier.cpp
    utils/bit_coding.h
    utils/sun_calculations.h utils/sun_calculations.cpp
    map_label/MapLabel.h map_label/MapLabel.cpp
    map_label/MapLabelManager.h map_label/MapLabelManager.cpp
    utils/bit_coding.h
    tile_scheduler/cache_quieries.h
    DataQuerier.h DataQuerier.cpp
    camera/LinearCameraAnimation.h camera/LinearCameraAnimation.cpp
    camera/AnimationStyle.h camera/AnimationStyle.cpp
    timing/TimerManager.h timing/TimerManager.cpp
    timing/TimerInterface.h timing/TimerInterface.cpp
    timing/CpuTimer.h timing/CpuTimer.cpp
    GPX.h GPX.cpp
)

target_include_directories(nucleus PRIVATE . PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(nucleus PUBLIC radix Qt::Core Qt::Gui Qt::Network Qt::Svg fmt::fmt zppbits tl_expected nucleus_version stb_slim)

# if (EMSCRIPTEN)
#     # target_compile_options(nucleus PUBLIC -fwasm-exceptions)
#     # target_link_options(nucleus PUBLIC -fwasm-exceptions)
# endif()
if (ALP_ENABLE_THREADING)
    target_compile_definitions(nucleus PUBLIC "ALP_ENABLE_THREADING")
endif()

if (MSVC)
    target_compile_options(nucleus PUBLIC /W4 #[[/WX]])
    # /WX fails with an unreachable code warning/error in zpp_bits.h. the system property doesn't seem to work (even though it appears in the build log as
    # -external:ID:\a\renderer\renderer\extern\zppbits -external:W0
else()
    target_compile_options(nucleus PUBLIC -Wall -Wextra -pedantic -Werror)
    if (EMSCRIPTEN)
        target_compile_options(nucleus PUBLIC -Wno-dollar-in-identifier-extension)
    endif()
endif()
