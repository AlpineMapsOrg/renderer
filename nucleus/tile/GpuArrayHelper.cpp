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
#include <QtGlobal>
#include <nucleus/srs.h>

namespace nucleus::tile {
GpuArrayHelper::GpuArrayHelper() { }

uint16_t GpuArrayHelper::add_tile(const tile::Id& id)
{
    const auto existing = m_id_to_layer.find(id);
    Q_ASSERT(existing == m_id_to_layer.end());
    if (existing != m_id_to_layer.end())
        return existing->second;

    if (m_free_layers.empty())
        return invalid_layer;

    const auto layer = m_free_layers.back();
    m_free_layers.pop_back();
    m_id_to_layer.emplace(id, layer);
    return layer;
}

void GpuArrayHelper::remove_tile(const tile::Id& tile_id)
{
    const auto layer_it = m_id_to_layer.find(tile_id);

    // TODO: should this assert or be a noop?
    Q_ASSERT(layer_it != m_id_to_layer.end());
    if (layer_it == m_id_to_layer.end())
        return;

    m_free_layers.push_back(layer_it->second);
    m_id_to_layer.erase(layer_it);
}

void GpuArrayHelper::set_tile_limit(unsigned int new_limit)
{
    Q_ASSERT(m_size == 0);
    Q_ASSERT(new_limit <= invalid_layer);  
    if (new_limit > invalid_layer)
        new_limit = invalid_layer;

    m_size = new_limit;
    m_free_layers.resize(new_limit);
    // fill freelist
    for (unsigned i = 0; i < new_limit; ++i)
        m_free_layers[i] = uint16_t(new_limit - 1 - i);

    m_id_to_layer.reserve(new_limit);
}

unsigned GpuArrayHelper::size() const { return m_size; }

unsigned GpuArrayHelper::n_occupied() const { return unsigned(m_id_to_layer.size()); }

GpuArrayHelper::LayerInfo GpuArrayHelper::layer(Id tile_id) const
{
    auto it = m_id_to_layer.find(tile_id);
    while (it == m_id_to_layer.end() && tile_id.zoom_level > 0) {
        tile_id = tile_id.parent();
        it = m_id_to_layer.find(tile_id);
    }
    if (it == m_id_to_layer.end())
        return { {}, 0 }; // may be empty during startup.
    return { tile_id, it->second };
}

bool GpuArrayHelper::contains(Id tile_id) const { return m_id_to_layer.contains(tile_id); }

GpuArrayHelper::Dictionary GpuArrayHelper::generate_dictionary() const
{
    const auto hash_to_pixel = [](uint16_t hash) { return glm::uvec2(hash & 255, hash >> 8); };
    radix::Raster<glm::u32vec2> packed_ids({ 256, 256 }, glm::u32vec2(-1, -1));
    radix::Raster<uint16_t> layers({ 256, 256 }, 0);
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
