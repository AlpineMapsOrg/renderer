/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2023 Adam Celarek
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#pragma once

#include "nucleus/srs.h"

#include <QtAssert>
#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include "nucleus/tile/Cache.h"
#include "radix/height_encoding.h"

#include "nucleus/utils/image_loader.h"

namespace nucleus::tile::cache_queries {

inline std::expected<float, QString> query_altitude(MemoryCache* cache, const glm::dvec2& lat_long)
{
    const auto world_space = srs::lat_long_to_world(lat_long);
    nucleus::tile::Data selected_tile;
    cache->visit([&](const nucleus::tile::DataQuad& tile) {
        for (const auto& t : tile.tiles) {
            if (srs::tile_bounds(t.id).contains(world_space) && t.network_info.status == NetworkInfo::Status::Good) {
                selected_tile = t;
                return true;
            }
        }
        return false;
    });
    if (!selected_tile.data)
        return std::unexpected(QString("Couldn't find altitude for %1/%2").arg(lat_long.x).arg(lat_long.y));
    Q_ASSERT(selected_tile.data->size());

    const auto bounds = srs::tile_bounds(selected_tile.id);
    const auto uv = (world_space - bounds.min) / bounds.size();

    if (selected_tile.data && selected_tile.data->size()) {
        if (const auto height_tile_expected = nucleus::utils::image_loader::rgba8(*selected_tile.data)) {
            const auto& height_tile = height_tile_expected.value();
            const auto p = glm::uvec2(uint32_t(uv.x * height_tile.width()), uint32_t((1 - uv.y) * height_tile.height()));
            const auto px = height_tile.pixel(p);
            return radix::height_encoding::to_float(glm::u8vec3(px));
        }
    }
    Q_ASSERT(false);
    return std::unexpected(QString("Couldn't find altitude for %1/%2").arg(lat_long.x).arg(lat_long.y));
}

// like query_altitude, but bilinearly interpolates between the dem texels.
// the rendered terrain interpolates the height texture the same way, so
// altitudes sampled with this function match the visible surface (the
// nearest-texel variant staircase-steps against it on steep ground)
inline std::expected<float, QString> query_altitude_bilinear(MemoryCache* cache, const glm::dvec2& lat_long)
{
    const auto world_space = srs::lat_long_to_world(lat_long);
    nucleus::tile::Data selected_tile;
    cache->visit([&](const nucleus::tile::DataQuad& tile) {
        for (const auto& t : tile.tiles) {
            if (srs::tile_bounds(t.id).contains(world_space) && t.network_info.status == NetworkInfo::Status::Good) {
                selected_tile = t;
                return true;
            }
        }
        return false;
    });
    if (!selected_tile.data)
        return std::unexpected(QString("Couldn't find altitude for %1/%2").arg(lat_long.x).arg(lat_long.y));
    Q_ASSERT(selected_tile.data->size());

    const auto bounds = srs::tile_bounds(selected_tile.id);
    const auto uv = (world_space - bounds.min) / bounds.size();

    if (selected_tile.data && selected_tile.data->size()) {
        if (const auto height_tile_expected = nucleus::utils::image_loader::rgba8(*selected_tile.data)) {
            const auto& height_tile = height_tile_expected.value();
            // texel centres lie at (i + 0.5) / size; v is flipped (row 0 = north)
            const double fx = uv.x * double(height_tile.width()) - 0.5;
            const double fy = (1.0 - uv.y) * double(height_tile.height()) - 0.5;
            const double x0 = std::clamp(std::floor(fx), 0.0, double(height_tile.width()) - 1.0);
            const double y0 = std::clamp(std::floor(fy), 0.0, double(height_tile.height()) - 1.0);
            const double x1 = std::min(x0 + 1.0, double(height_tile.width()) - 1.0);
            const double y1 = std::min(y0 + 1.0, double(height_tile.height()) - 1.0);
            const double tx = std::clamp(fx - x0, 0.0, 1.0);
            const double ty = std::clamp(fy - y0, 0.0, 1.0);

            const auto sample = [&height_tile](double px, double py) {
                return radix::height_encoding::to_float(glm::u8vec3(height_tile.pixel(glm::uvec2(uint32_t(px), uint32_t(py)))));
            };
            const double top = sample(x0, y0) * (1.0 - tx) + sample(x1, y0) * tx;
            const double bottom = sample(x0, y1) * (1.0 - tx) + sample(x1, y1) * tx;
            return float(top * (1.0 - ty) + bottom * ty);
        }
    }
    Q_ASSERT(false);
    return std::unexpected(QString("Couldn't find altitude for %1/%2").arg(lat_long.x).arg(lat_long.y));
}

// altitude of the *rendered* terrain surface of an already decoded tile: the
// tile mesh is a triangle strip over the texel grid (diagonal from the
// south-western to the north-eastern cell corner, see
// terrain_mesh_index_generator), so the surface between four texels is two
// triangles. sampling this instead of a bilinear patch makes altitudes match
// the drawn terrain, also diagonal to the grid. nullopt when the position is
// outside the tile
inline std::optional<float> sample_tile_mesh(const radix::Raster<glm::u8vec4>& height_tile,
                                             const tile::Id& id,
                                             const glm::dvec2& world_space)
{
    const auto bounds = srs::tile_bounds(id);
    if (!bounds.contains(world_space)) {
        return std::nullopt;
    }
    const auto uv = (world_space - bounds.min) / bounds.size();

    // vertex grid spans the tile with (width-1) x (height-1) cells; row 0 is
    // the north edge
    const double fx = uv.x * double(height_tile.width() - 1);
    const double fy = (1.0 - uv.y) * double(height_tile.height() - 1);
    const double cx = std::clamp(std::floor(fx), 0.0, double(height_tile.width()) - 2.0);
    const double cy = std::clamp(std::floor(fy), 0.0, double(height_tile.height()) - 2.0);
    const double tx = std::clamp(fx - cx, 0.0, 1.0);
    const double ty = std::clamp(fy - cy, 0.0, 1.0);

    const auto texel = [&height_tile](double px, double py) {
        return radix::height_encoding::to_float(glm::u8vec3(height_tile.pixel(glm::uvec2(uint32_t(px), uint32_t(py)))));
    };
    const double north_west = texel(cx, cy);
    const double north_east = texel(cx + 1, cy);
    const double south_west = texel(cx, cy + 1);
    const double south_east = texel(cx + 1, cy + 1);

    if (tx + ty <= 1.0) {
        return float(north_west + (north_east - north_west) * tx + (south_west - north_west) * ty);
    }
    return float(south_east + (south_west - south_east) * (1.0 - tx) + (north_east - south_east) * (1.0 - ty));
}

inline std::expected<float, QString> query_mesh_altitude(MemoryCache* cache, const glm::dvec2& lat_long)
{
    const auto world_space = srs::lat_long_to_world(lat_long);
    nucleus::tile::Data selected_tile;
    cache->visit([&](const nucleus::tile::DataQuad& tile) {
        for (const auto& t : tile.tiles) {
            if (srs::tile_bounds(t.id).contains(world_space) && t.network_info.status == NetworkInfo::Status::Good) {
                selected_tile = t;
                return true;
            }
        }
        return false;
    });
    if (!selected_tile.data)
        return std::unexpected(QString("Couldn't find altitude for %1/%2").arg(lat_long.x).arg(lat_long.y));

    if (const auto height_tile_expected = nucleus::utils::image_loader::rgba8(*selected_tile.data)) {
        const auto sampled = sample_tile_mesh(height_tile_expected.value(), selected_tile.id, world_space);
        if (sampled) {
            return *sampled;
        }
    }
    Q_ASSERT(false);
    return std::unexpected(QString("Couldn't find altitude for %1/%2").arg(lat_long.x).arg(lat_long.y));
}

// decodes each dem tile once and samples the *rendered* terrain surface from
// the decoded image (same triangle-mesh semantics as query_mesh_altitude).
// query_mesh_altitude decodes the tile image for every single query, which is
// far too slow for sampling a whole route line: a 500-sample segment would
// decode the same tile 500 times. a last-tile shortcut skips the cache lookup
// for spatially coherent queries (route samples are ordered along the line);
// the decode map handles queries alternating across a tile boundary.
// not synchronised: create one instance per worker thread.
class DemMeshSampler {
public:
    explicit DemMeshSampler(MemoryCache* cache)
        : m_cache(cache)
    {
    }

    // altitude of the rendered terrain surface at the position
    std::expected<float, QString> altitude(const glm::dvec2& lat_long)
    {
        const glm::dvec2 world_space = srs::lat_long_to_world(lat_long);
        const DecodedTile* tile = decoded_tile(world_space);
        if (!tile) {
            return std::unexpected(QString("Couldn't find altitude for %1/%2").arg(lat_long.x).arg(lat_long.y));
        }
        const auto sampled = sample_tile_mesh(tile->image, tile->id, world_space);
        if (!sampled) {
            return std::unexpected(QString("Couldn't find altitude for %1/%2").arg(lat_long.x).arg(lat_long.y));
        }
        return *sampled;
    }

    // metres per dem cell of the terrain displayed at the position (the tile
    // the cache serves there), for spacing interpolation steps at one cell
    std::expected<double, QString> cell_size(const glm::dvec2& lat_long)
    {
        const DecodedTile* tile = decoded_tile(srs::lat_long_to_world(lat_long));
        if (!tile) {
            return std::unexpected(QString("Couldn't find dem resolution for %1/%2").arg(lat_long.x).arg(lat_long.y));
        }
        return srs::tile_width(int(tile->id.zoom_level)) / double(tile->image.width());
    }

private:
    struct DecodedTile {
        tile::Id id;
        radix::Raster<glm::u8vec4> image;
    };

    // finds the tile covering the world position, decoding it on first use
    const DecodedTile* decoded_tile(const glm::dvec2& world_space)
    {
        if (m_last && srs::tile_bounds(m_last->id).contains(world_space)) {
            return &*m_last;
        }
        m_last.reset();
        if (!m_cache) {
            return nullptr;
        }
        nucleus::tile::Data selected_tile;
        m_cache->visit([&](const nucleus::tile::DataQuad& tile) {
            for (const auto& t : tile.tiles) {
                if (srs::tile_bounds(t.id).contains(world_space) && t.network_info.status == NetworkInfo::Status::Good) {
                    selected_tile = t;
                    return true;
                }
            }
            return false;
        });
        if (!selected_tile.data) {
            return nullptr;
        }

        // tile coordinates fit 28 bits each (zoom <= 2^28 tiles per axis)
        const uint64_t key = (uint64_t(selected_tile.id.zoom_level) << 56)
            | (uint64_t(selected_tile.id.coords.x) << 28) | uint64_t(selected_tile.id.coords.y);
        auto decoded = m_decoded.find(key);
        if (decoded == m_decoded.end()) {
            auto image = nucleus::utils::image_loader::rgba8(*selected_tile.data);
            if (!image) {
                return nullptr;
            }
            decoded = m_decoded.emplace(key, DecodedTile { selected_tile.id, std::move(image.value()) }).first;
        }
        m_last = decoded->second;
        return &*m_last;
    }

    MemoryCache* m_cache = nullptr;
    std::optional<DecodedTile> m_last;
    std::unordered_map<uint64_t, DecodedTile> m_decoded;
};

} // namespace nucleus::tile::cache_queries
