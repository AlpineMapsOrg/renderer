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

#pragma once

#include "Node.h"

namespace webgpu_compute::nodes {

class SelectTilesNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(SelectTilesNode)

    struct SelectTilesNodeSettings {
        uint32_t zoomlevel = 15;
    };

    SelectTilesNode();

    void set_settings(const SelectTilesNodeSettings& settings) { m_settings = settings; }
    const SelectTilesNodeSettings& get_settings() const { return m_settings; }
    void serialize_settings(QJsonObject& out) const override;
    void deserialize_settings(const QJsonObject& in) override;

public slots:
    void run_impl() override;

private:
    SelectTilesNodeSettings m_settings;
    std::vector<radix::tile::Id> m_output_tile_ids;
    radix::geometry::Aabb<2, double> m_output_bounds;

    radix::geometry::Aabb<3, double> m_cached_region;
    uint32_t m_cached_zoom = 0;
    bool m_has_cached = false;
};
} // namespace webgpu_compute::nodes
