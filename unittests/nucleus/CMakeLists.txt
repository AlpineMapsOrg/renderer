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

project(alpine-renderer-unittests_nucleus LANGUAGES CXX)

alp_add_unittest(unittests_nucleus
    catch2_helpers.h
    Camera.cpp
    utils_stopwatch.cpp
    DrawListGenerator.cpp
    test_helpers.h test_helpers.cpp
    raster.cpp
    terrain_mesh_index_generator.cpp
    srs.cpp
    track.cpp
    tile_conversion.cpp
    tile_scheduler_util.cpp
    tile_scheduler_tile_load_service.cpp
    tile_scheduler_layer_assembler.cpp
    tile_scheduler_quad_assembler.cpp
    tile_scheduler_cache.cpp
    tile_scheduler_scheduler.cpp
    tile_scheduler_slot_limiter.cpp
    tile_scheduler_rate_limiter.cpp
    RateTester.h RateTester.cpp
    zppbits.cpp
    cache_queries.cpp
    bits_and_pieces.cpp
)

if (ALP_ENABLE_AVLANCHE_WARNING_LAYER)
    target_sources(unittests_nucleus
      PRIVATE
        avalanche_warning_layer.cpp
    )
endif()

if (ALP_ENABLE_LABELS)
    target_sources(unittests_nucleus PRIVATE
        vector_tile.cpp
        map_labels.cpp
    )
endif()

qt_add_resources(unittests_nucleus "test_data"
    PREFIX "/test_data"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/data/
    FILES
    data/170px-Jeune_bouquetin_de_face.jpg
    data/test-tile_ortho.jpeg
    data/test-tile.png
    data/example.gpx
    data/vectortile.mvt
    data/eaws_0-0-0.mvt
    data/eaws_2-2-0.mvt
    data/eaws_10-236-299.mvt
)
target_link_libraries(unittests_nucleus PUBLIC nucleus Catch2::Catch2 Qt::Test Qt::Gui)
target_compile_definitions(unittests_nucleus PUBLIC "ALP_TEST_DATA_DIR=\":/test_data/\"")

if (ANDROID)
    add_android_openssl_libraries(unittests_nucleus)
endif()

