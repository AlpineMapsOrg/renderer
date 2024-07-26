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
    test_Camera.cpp
    nucleus_utils_stopwatch.cpp
    test_DrawListGenerator.cpp
    test_helpers.h test_helpers.cpp
    test_raster.cpp
    test_terrain_mesh_index_generator.cpp
    test_srs.cpp
    test_tile_conversion.cpp
    test_vector_tile.cpp
    nucleus_tile_scheduler_util.cpp
    nucleus_tile_scheduler_tile_load_service.cpp
    nucleus_tile_scheduler_layer_assembler.cpp
    nucleus_tile_scheduler_quad_assembler.cpp
    nucleus_tile_scheduler_cache.cpp
    nucleus_tile_scheduler_scheduler.cpp
    nucleus_tile_scheduler_slot_limiter.cpp
    nucleus_tile_scheduler_rate_limiter.cpp
    RateTester.h RateTester.cpp
    test_zppbits.cpp
    cache_queries.cpp
    bits_and_pieces.cpp
)

if (ALP_ENABLE_AVLANCHE_WARNING_LAYER)
    target_sources(unittests_nucleus
      PRIVATE
        avalanche_warning_layer.cpp
    )
endif()

qt_add_resources(unittests_nucleus "test_data"
    PREFIX "/test_data"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/data/
    FILES
    data/170px-Jeune_bouquetin_de_face.jpg
    data/test-tile_ortho.jpeg
    data/test-tile.png
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

