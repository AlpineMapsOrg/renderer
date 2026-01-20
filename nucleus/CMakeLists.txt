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

alp_add_git_repository(stb_slim URL https://github.com/AlpineMapsOrgDependencies/stb_slim.git COMMITISH 547fade2a12793e1bea4733d59646b4f436e25a4)
alp_add_git_repository(radix URL https://github.com/AlpineMapsOrg/radix.git COMMITISH e939e10c5a40866950b68a0bc04c851bcfcf5dad NOT_SYSTEM)
alp_add_git_repository(tl_expected URL https://github.com/TartanLlama/expected.git COMMITISH v1.1.0 DO_NOT_ADD_SUBPROJECT)
alp_add_git_repository(zppbits URL https://github.com/eyalz800/zpp_bits.git COMMITISH v4.4.25 DO_NOT_ADD_SUBPROJECT)
if(ALP_ENABLE_LABELS)
    alp_add_git_repository(vector_tiles URL https://github.com/AlpineMapsOrg/vector-tile.git COMMITISH faba88257716c4bc01ebd44d8b8b98f711ecb78c)
endif()
alp_add_git_repository(goofy_tc URL https://github.com/AlpineMapsOrgDependencies/Goofy_slim.git COMMITISH 13b228784960a6227bb6ca704ff34161bbac1b91 DO_NOT_ADD_SUBPROJECT)

add_library(zppbits INTERFACE)
target_include_directories(zppbits SYSTEM INTERFACE ${zppbits_SOURCE_DIR})

add_library(goofy_tc INTERFACE)
target_include_directories(goofy_tc INTERFACE ${goofy_tc_SOURCE_DIR})
set_target_properties(goofy_tc PROPERTIES SYSTEM true)

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
    AbstractRenderWindow.h
    event_parameter.h
    Raster.h
    srs.h srs.cpp
    tile/utils.h tile/utils.cpp
    tile/DrawListGenerator.h tile/DrawListGenerator.cpp
    tile/types.h
    tile/constants.h
    tile/QuadAssembler.h tile/QuadAssembler.cpp
    tile/Cache.h
    tile/TileLoadService.h tile/TileLoadService.cpp
    tile/Scheduler.h tile/Scheduler.cpp
    tile/SlotLimiter.h tile/SlotLimiter.cpp
    tile/RateLimiter.h tile/RateLimiter.cpp
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
    tile/conversion.h tile/conversion.cpp
    utils/UrlModifier.h utils/UrlModifier.cpp
    utils/bit_coding.h
    utils/sun_calculations.h utils/sun_calculations.cpp
    picker/PickerManager.h picker/PickerManager.cpp
    picker/types.h
    utils/bit_coding.h
    tile/cache_quieries.h
    DataQuerier.h DataQuerier.cpp
    camera/LinearCameraAnimation.h camera/LinearCameraAnimation.cpp
    camera/AnimationStyle.h camera/AnimationStyle.cpp
    timing/TimerManager.h timing/TimerManager.cpp
    timing/TimerInterface.h timing/TimerInterface.cpp
    timing/CpuTimer.h timing/CpuTimer.cpp
    utils/ColourTexture.h utils/ColourTexture.cpp
    EngineContext.h EngineContext.cpp
    track/Manager.h track/Manager.cpp
    track/GPX.cpp
    track/GPX.h
    utils/image_loader.h utils/image_loader.cpp
    utils/image_writer.h utils/image_writer.cpp
    utils/thread.h
    camera/RecordedAnimation.h camera/RecordedAnimation.cpp
    camera/recording.h camera/recording.cpp
    tile/setup.h
    tile/GpuArrayHelper.h tile/GpuArrayHelper.cpp
    tile/TextureScheduler.h tile/TextureScheduler.cpp
    tile/GeometryScheduler.h tile/GeometryScheduler.cpp
    utils/error.h
    utils/lang.h
    tile/SchedulerDirector.h tile/SchedulerDirector.cpp
    tile/drawing.h tile/drawing.cpp
    camera/gesture.h
)

if (ALP_ENABLE_AVLANCHE_WARNING_LAYER)
    target_sources(nucleus
        PUBLIC avalanche/eaws.h avalanche/eaws.cpp
    )
    target_link_libraries(nucleus PUBLIC Qt::Gui)
endif()
if(ALP_ENABLE_LABELS)
    target_sources(nucleus PRIVATE
        vector_tile/util.h
        vector_tile/types.h
        vector_tile/parse.h vector_tile/parse.cpp
        map_label/Factory.h map_label/Factory.cpp
        map_label/types.h
        map_label/FontRenderer.h map_label/FontRenderer.cpp
        map_label/Filter.h map_label/Filter.cpp
        map_label/FilterDefinitions.h
        map_label/Scheduler.h map_label/Scheduler.cpp
        map_label/setup.h
    )
    target_link_libraries(nucleus PUBLIC vector_tiles Qt::Gui)
    target_compile_definitions(nucleus PUBLIC ALP_ENABLE_LABELS)
endif()

if (ALP_ENABLE_DEV_TOOLS)
    target_compile_definitions(nucleus PUBLIC ALP_ENABLE_DEV_TOOLS)
endif()

target_include_directories(nucleus PUBLIC ${CMAKE_SOURCE_DIR})
# Please keep Qt::Gui outside the nucleus. If you need it optional via a cmake based switch
target_link_libraries(nucleus PUBLIC radix Qt::Core Qt::Network zppbits tl_expected nucleus_version stb_slim goofy_tc)

qt_add_resources(nucleus "icons"
    PREFIX "/map_icons"
    BASE ${CMAKE_SOURCE_DIR}/app/icons/labels
    FILES
        ${CMAKE_SOURCE_DIR}/app/icons/labels/alpinehut.png
        ${CMAKE_SOURCE_DIR}/app/icons/labels/city.png
        ${CMAKE_SOURCE_DIR}/app/icons/labels/peak.png
        ${CMAKE_SOURCE_DIR}/app/icons/labels/camera.png
)
qt_add_resources(nucleus "height_data"
    PREFIX "/map"
    BASE ${renderer_static_data_SOURCE_DIR}
    FILES ${renderer_static_data_SOURCE_DIR}/height_data.atb
)
qt_add_resources(nucleus "nucleus_fonts"
    BASE ${alpineapp_fonts_SOURCE_DIR}/
    PREFIX "/fonts"
    FILES
    ${alpineapp_fonts_SOURCE_DIR}/Roboto/Roboto-Bold.ttf
)

if (ALP_ENABLE_LTO)
    target_compile_options(nucleus PUBLIC -flto)
    target_link_options(nucleus PUBLIC -flto)
endif()


if (EMSCRIPTEN)
    target_compile_options(nucleus PUBLIC -msimd128 -msse2 -Wno-dollar-in-identifier-extension)
#     # target_compile_options(nucleus PUBLIC -fwasm-exceptions)
#     # target_link_options(nucleus PUBLIC -fwasm-exceptions)
endif()
if (ALP_ENABLE_THREADING)
    target_compile_definitions(nucleus PUBLIC ALP_ENABLE_THREADING)
endif()

if (MSVC)
    target_compile_options(nucleus PUBLIC /W4 #[[/WX]])
    # /WX fails with an unreachable code warning/error in zpp_bits.h. the system property doesn't seem to work (even though it appears in the build log as
    # "-external:ID:\a\renderer\renderer\extern\zppbits -external:W0")
else()
    # imgui does not compile with -Werror. so we have to make it private for now.
    # better solutions welcome
    target_compile_options(nucleus PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
