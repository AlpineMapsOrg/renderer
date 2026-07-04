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

#include "webgpu/compute/RectangularTileRegion.h"
#include <QDebug>
#include <nucleus/srs.h>

namespace webgpu_compute::nodes {

SelectTilesNode::SelectTilesNode()
    : Node({ InputSocket(*this, "region", data_type<const radix::geometry::Aabb<3, double>*>()) },
          {
              OutputSocket(*this, "tile ids", data_type<const std::vector<radix::tile::Id>*>(), [this]() { return &m_output_tile_ids; }),
              OutputSocket(*this, "region aabb", data_type<const radix::geometry::Aabb<2, double>*>(), [this]() { return &m_output_bounds; }),
          })
    , m_output_bounds { glm::dvec2(std::numeric_limits<double>::max()), glm::dvec2(std::numeric_limits<double>::min()) }
{
}

void SelectTilesNode::run_impl()
{
    if (!input_socket("region").is_socket_connected()) {
        fail_run("no region input connected");
        return;
    }

    const auto* region = std::get<data_type<const radix::geometry::Aabb<3, double>*>()>(input_socket("region").get_connected_data());

    if (m_has_cached && *region == m_cached_region && m_settings.zoomlevel == m_cached_zoom) {
        complete_run();
        return;
    }

    const auto lower_left_tile = nucleus::srs::world_xy_to_tile_id(glm::dvec2(region->min), m_settings.zoomlevel);
    const auto upper_right_tile = nucleus::srs::world_xy_to_tile_id(glm::dvec2(region->max), m_settings.zoomlevel);

    webgpu_compute::RectangularTileRegion tile_region {
        .min = lower_left_tile.coords,
        .max = upper_right_tile.coords,
        .zoom_level = upper_right_tile.zoom_level,
        .scheme = radix::tile::Scheme::Tms,
    };
    const auto tile_ids = tile_region.get_tiles();

    m_output_tile_ids.clear();
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

    m_cached_region = *region;
    m_cached_zoom = m_settings.zoomlevel;
    m_has_cached = true;

    complete_run();
}

void SelectTilesNode::serialize_settings(QJsonObject& out) const { out["zoomlevel"] = static_cast<int>(m_settings.zoomlevel); }

void SelectTilesNode::deserialize_settings(const QJsonObject& in)
{
    if (in.contains("zoomlevel"))
        m_settings.zoomlevel = static_cast<uint32_t>(in["zoomlevel"].toInt(static_cast<int>(m_settings.zoomlevel)));
}

} // namespace webgpu_compute::nodes
