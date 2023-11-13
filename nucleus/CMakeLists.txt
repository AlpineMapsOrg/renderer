cmake_minimum_required(VERSION 3.21)
project(alpine-renderer-nucleus LANGUAGES CXX)

option(ALP_AUTOUPDATE_RADIX "Keeps whack up-to-date with origin/main, but prevents local edits. Change to OFF if you want to edit radix" TRUE)

include(FetchContent)

message("ALP_EXTERN_DIR=${ALP_EXTERN_DIR}")
if(ALP_AUTOUPDATE_RADIX)
    FetchContent_Declare(radix
        GIT_REPOSITORY  https://github.com/AlpineMapsOrg/renderer.git
        GIT_TAG         origin/main
        SOURCE_DIR      ${CMAKE_SOURCE_DIR}/extern/radix
        SOURCE_SUBDIR   src
    )
    FetchContent_MakeAvailable(radix)
else()
    if (NOT EXISTS ${CMAKE_SOURCE_DIR}/extern/radix)
        FetchContent_Declare(radix
            GIT_REPOSITORY  git@github.com:AlpineMapsOrg/radix.git
            GIT_TAG         main
            SOURCE_DIR      ${CMAKE_SOURCE_DIR}/extern/radix
        )
        FetchContent_MakeAvailable(radix)
    else()
        add_subdirectory(${CMAKE_SOURCE_DIR}/extern/radix ${CMAKE_BINARY_DIR}/_deps/radix-build)
    endif()
endif()

set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "Enable tl::expected tests")
FetchContent_Declare(tl_expected
    URL      https://github.com/TartanLlama/expected/archive/refs/tags/v1.1.0.zip
    URL_HASH MD5=cbc9465bb0e9328c821fc3cf89ec7711
    DOWNLOAD_EXTRACT_TIMESTAMP true
    SOURCE_DIR      ${CMAKE_SOURCE_DIR}/extern/tl_expected
)
FetchContent_MakeAvailable(tl_expected)

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
    map_label/MapLabelModel.h map_label/MapLabelModel.cpp
    map_label/CameraTransformationProxyModel.h map_label/CameraTransformationProxyModel.cpp
    map_label/MapLabel.h
    map_label/AbstractMapLabelModel.h
    map_label/CameraFilterProxyModel.h map_label/CameraFilterProxyModel.cpp
    utils/bit_coding.h
    tile_scheduler/cache_quieries.h
    DataQuerier.h DataQuerier.cpp
    camera/LinearCameraAnimation.h camera/LinearCameraAnimation.cpp
    camera/AnimationStyle.h camera/AnimationStyle.cpp
    timing/TimerManager.h timing/TimerManager.cpp
    timing/TimerInterface.h timing/TimerInterface.cpp
    timing/CpuTimer.h timing/CpuTimer.cpp
)

target_include_directories(nucleus PRIVATE . PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(nucleus PUBLIC sherpa Qt::Core Qt::Gui Qt::Network Qt::Svg fmt::fmt zppbits expected)


if (EMSCRIPTEN)
    target_compile_options(nucleus PUBLIC -O3)
    target_link_options(nucleus PUBLIC -O3)
endif()
if (ALP_ENABLE_ASSERTS)
    target_compile_options(nucleus PUBLIC "-U NDEBUG")
endif()
if (ALP_ENABLE_THREADING)
    target_compile_definitions(nucleus PUBLIC "ALP_ENABLE_THREADING")
endif()
