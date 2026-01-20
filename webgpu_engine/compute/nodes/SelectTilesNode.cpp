/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include "SelectTilesNode.h"

#include "webgpu_engine/compute/RectangularTileRegion.h"
#include <QDebug>
#include <nucleus/srs.h>

namespace webgpu_engine::compute::nodes {

SelectTilesNode::SelectTilesNode()
    : SelectTilesNode([]() { return std::vector<radix::tile::Id> {}; })
{
}

SelectTilesNode::SelectTilesNode(TileIdGenerator tile_id_generator)
    : Node({},
          {
              OutputSocket(*this, "tile ids", data_type<const std::vector<radix::tile::Id>*>(), [this]() { return &m_output_tile_ids; }),
              OutputSocket(*this, "region aabb", data_type<const radix::geometry::Aabb<2, double>*>(), [this]() { return &m_output_bounds; }),
          })
    , m_tile_id_generator(tile_id_generator)
    , m_output_bounds { glm::dvec2(std::numeric_limits<double>::max()), glm::dvec2(std::numeric_limits<double>::min()) }
{
}

void SelectTilesNode::set_tile_id_generator(TileIdGenerator tile_id_generator) { m_tile_id_generator = tile_id_generator; }

void SelectTilesNode::select_tiles_in_world_aabb(const radix::geometry::Aabb<3, double>& aabb, unsigned int zoomlevel)
{
    const auto lower_left_tile = nucleus::srs::world_xy_to_tile_id(glm::dvec2(aabb.min), zoomlevel);
    const auto upper_right_tile = nucleus::srs::world_xy_to_tile_id(glm::dvec2(aabb.max), zoomlevel);

    set_tile_id_generator([lower_left_tile, upper_right_tile]() {
        compute::RectangularTileRegion region {
            .min = lower_left_tile.coords,
            .max = upper_right_tile.coords,
            .zoom_level = upper_right_tile.zoom_level,
            .scheme = radix::tile::Scheme::Tms,
        };
        return region.get_tiles();
    });
}

void SelectTilesNode::run_impl()
{
    qDebug() << "running TileSelectNode ...";
    m_output_tile_ids.clear();
    const auto& tile_ids = m_tile_id_generator();

    if (tile_ids.empty()) {
        qWarning() << "no tiles selected";
        return;
    }
    qDebug() << tile_ids.size() << " tiles selected";

    m_output_bounds = { glm::dvec2(std::numeric_limits<double>::max()), glm::dvec2(std::numeric_limits<double>::min()) };
    for (const auto& tile_id : tile_ids) {
        m_output_bounds.expand_by(nucleus::srs::tile_bounds(tile_id));
    }
    qDebug() << Qt::fixed << "selected aabb=[(" << m_output_bounds.min.x << ", " << m_output_bounds.min.y << "), (" << m_output_bounds.max.x << ", "
             << m_output_bounds.max.y << ")]";

    m_output_tile_ids.insert(m_output_tile_ids.begin(), tile_ids.begin(), tile_ids.end());
    emit run_completed();
}

} // namespace webgpu_engine::compute::nodes
