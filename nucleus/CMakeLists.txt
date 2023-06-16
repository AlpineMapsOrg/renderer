cmake_minimum_required(VERSION 3.21)
project(alpine-renderer-nucleus LANGUAGES CXX)

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
    camera/RotateNorthInteraction.h camera/RotateNorthInteraction.cpp
    camera/AbstractDepthTester.h
    camera/stored_positions.h
    utils/Stopwatch.h utils/Stopwatch.cpp
    utils/terrain_mesh_index_generator.h
    utils/tile_conversion.h utils/tile_conversion.cpp
    map_label/MapLabelModel.h map_label/MapLabelModel.cpp
    map_label/CameraTransformationProxyModel.h map_label/CameraTransformationProxyModel.cpp
    map_label/MapLabel.h
    map_label/AbstractMapLabelModel.h
    map_label/CameraFilterProxyModel.h map_label/CameraFilterProxyModel.cpp
    utils/bit_coding.h
)

target_include_directories(nucleus PRIVATE . PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(nucleus PUBLIC sherpa Qt::Core Qt::Gui Qt::Network Qt::Svg fmt::fmt zppbits)


if (EMSCRIPTEN)
    target_compile_options(nucleus PUBLIC -fexceptions -O3)
    target_link_options(nucleus PUBLIC -fexceptions -O3)
endif()
if (ALP_ENABLE_ASSERTS)
    target_compile_options(nucleus PUBLIC "-U NDEBUG")
endif()
if (ALP_ENABLE_THREADING)
    target_compile_definitions(nucleus PUBLIC "ALP_ENABLE_THREADING")
endif()
