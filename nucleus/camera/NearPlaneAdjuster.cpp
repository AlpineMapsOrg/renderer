/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
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

#include "NearPlaneAdjuster.h"

#include <functional>
#include <limits>

#include "Definition.h"
#include "nucleus/Tile.h"

using nucleus::camera::NearPlaneAdjuster;

NearPlaneAdjuster::NearPlaneAdjuster(QObject* parent)
    : QObject { parent }
{

}

void NearPlaneAdjuster::update_camera(const Definition& new_definition)
{
    //    const auto new_position = new_definition.position();
    //    if (m_camera_position == new_position)
    //        return;
    //    m_camera_position = new_position;
    //    update_near_plane();
}

void NearPlaneAdjuster::add_tile(const std::shared_ptr<nucleus::Tile>& tile)
{
    m_objects.emplace_back(tile->id, tile->bounds.max.z);
    update_near_plane();
}

void NearPlaneAdjuster::remove_tile(const tile::Id& tile_id)
{
    std::erase_if(m_objects, [&tile_id](const Object& o) { return o.id == tile_id; });
    update_near_plane();
}

void NearPlaneAdjuster::update_near_plane() const
{
    if (!m_objects.empty()) {
        double max_elevation = 0;
        for (const auto& obj : m_objects) {
            if (obj.elevation > max_elevation)
                max_elevation = obj.elevation;
        }
        double near_plane_distance = std::max(10.0, (m_camera_position.z - max_elevation) * 0.8);
        emit near_plane_changed(near_plane_distance);
    }
}
