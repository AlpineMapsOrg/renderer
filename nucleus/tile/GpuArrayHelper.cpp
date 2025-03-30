/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
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

#include "GpuArrayHelper.h"
#include <nucleus/srs.h>

namespace nucleus::tile {
GpuArrayHelper::GpuArrayHelper() { }

unsigned GpuArrayHelper::add_tile(const tile::Id& id)
{
    assert(!m_id_to_layer.contains(id));
    const auto t = std::find(m_array.begin(), m_array.end(), tile::Id { unsigned(-1), {} });
    assert(t != m_array.end());
    *t = id;

    // returns index in texture array
    const auto layer = unsigned(t - m_array.begin());
    m_id_to_layer.emplace(id, layer);
    return layer;
}

void GpuArrayHelper::remove_tile(const tile::Id& tile_id)
{
    assert(m_id_to_layer.contains(tile_id));
    m_id_to_layer.erase(tile_id);
    const auto t = std::find(m_array.begin(), m_array.end(), tile_id);
    assert(t != m_array.end()); // removing a tile that's not here. likely there is a race.
    *t = tile::Id { unsigned(-1), {} };
}

void GpuArrayHelper::set_tile_limit(unsigned int new_limit)
{
    assert(m_array.empty());
    m_array.resize(new_limit);
    std::fill(m_array.begin(), m_array.end(), tile::Id { unsigned(-1), {} });
}

unsigned GpuArrayHelper::size() const { return unsigned(m_array.size()); }

unsigned GpuArrayHelper::n_occupied() const { return unsigned(m_id_to_layer.size()); }

GpuArrayHelper::LayerInfo GpuArrayHelper::layer(Id tile_id) const
{
    while (!m_id_to_layer.contains(tile_id) && tile_id.zoom_level > 0)
        tile_id = tile_id.parent();
    if (!m_id_to_layer.contains(tile_id))
        return { {}, 0 }; // may be empty during startup.
    return { tile_id, m_id_to_layer.at(tile_id) };
}

GpuArrayHelper::Dictionary GpuArrayHelper::generate_dictionary() const
{
    const auto hash_to_pixel = [](uint16_t hash) { return glm::uvec2(hash & 255, hash >> 8); };
    nucleus::Raster<glm::u32vec2> packed_ids({ 256, 256 }, glm::u32vec2(-1, -1));
    nucleus::Raster<uint16_t> layers({ 256, 256 }, 0);
    for (const auto& [id, layer] : m_id_to_layer) {
        auto hash = nucleus::srs::hash_uint16(id);
        while (packed_ids.pixel(hash_to_pixel(hash)) != glm::u32vec2(-1, -1))
            hash++;

        packed_ids.pixel(hash_to_pixel(hash)) = nucleus::srs::pack(id);
        layers.pixel(hash_to_pixel(hash)) = layer;
    }

    return { packed_ids, layers };
}
} // namespace nucleus::tile
