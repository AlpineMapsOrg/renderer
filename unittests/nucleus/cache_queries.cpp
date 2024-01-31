/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include "nucleus/tile_scheduler/Cache.h"
#include "nucleus/tile_scheduler/Scheduler.h"
#include "nucleus/tile_scheduler/cache_quieries.h"
#include "nucleus/tile_scheduler/tile_types.h"
#include "radix/height_encoding.h"

#include <catch2/catch_test_macros.hpp>

#include <QBuffer>
#include <QImage>

using namespace nucleus::tile_scheduler;

namespace {

QByteArray png_tile(unsigned size, float altitude)
{
    QImage tile(QSize{int(size), int(size)}, QImage::Format_ARGB32);
    const auto rgb = radix::height_encoding::to_rgb(altitude);
    tile.fill(QColor(rgb.x, rgb.y, rgb.z));
    QByteArray arr;
    QBuffer buffer(&arr);
    buffer.open(QIODevice::WriteOnly);
    tile.save(&buffer, "PNG");
    return arr;
}

tile_types::TileQuad example_tile_quad_for(const tile::Id& id, float altitude)
{
    const auto children = id.children();
    tile_types::TileQuad cpu_quad;
    cpu_quad.id = id;
    cpu_quad.n_tiles = 4;
    const auto ortho_photo = Scheduler::white_jpeg_tile(256);
    const auto altitude_tile = png_tile(64, altitude);
    for (unsigned i = 0; i < 4; ++i) {
        cpu_quad.tiles[i].id = children[i];
        cpu_quad.tiles[i].ortho = std::make_shared<QByteArray>(ortho_photo);
        cpu_quad.tiles[i].height = std::make_shared<QByteArray>(altitude_tile);
        cpu_quad.tiles[i].network_info.status = tile_types::NetworkInfo::Status::Good;
        cpu_quad.tiles[i].network_info.timestamp = utils::time_since_epoch();
    }
    return cpu_quad;
}
} // namespace

TEST_CASE("cache_queries")
{
    Cache<tile_types::TileQuad> cache;
    cache.insert(example_tile_quad_for(tile::Id{0, {0, 0}}, 1000.0f));
    cache.insert(example_tile_quad_for(tile::Id{1, {0, 0}}, 3000.0f));
    cache.insert(example_tile_quad_for(tile::Id{1, {0, 1}}, 1000.0f));
    cache.insert(example_tile_quad_for(tile::Id{1, {1, 0}}, 1000.0f));
    cache.insert(example_tile_quad_for(tile::Id{1, {1, 1}}, 1000.0f));
    cache.insert(example_tile_quad_for(tile::Id{2, {2, 2}}, 1000.0f));
    cache.insert(example_tile_quad_for(tile::Id{3, {4, 5}}, 1000.0f));
    cache.insert(example_tile_quad_for(tile::Id{4, {8, 10}}, 2000.0f));

    CHECK(cache_queries::query_altitude(&cache, {47.5587933, -12.3450985}) == 1000);
    CHECK(cache_queries::query_altitude(&cache, {-47.5587933, -12.3450985}) == 3000);
    CHECK(cache_queries::query_altitude(&cache, {47.5587933, 12.3450985}) == 2000);
}
