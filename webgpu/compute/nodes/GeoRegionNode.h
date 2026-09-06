/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
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

/// Produces a region AABB from geographic coordinates, as an alternative to deriving one
/// from a GPX track. This is what lets a scenario be pure data - a latitude, a longitude
/// and an extent - instead of requiring a recorded track for every location of interest.
class GeoRegionNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(GeoRegionNode)

    struct GeoRegionSettings {
        double center_latitude = 47.77663;
        double center_longitude = 15.81600;
        float extent = 2500.0f; // edge length of the requested square [m]

        /* Altitude range of the emitted 3D box. Tile selection only uses the horizontal
         * extent, so this just needs to bracket the terrain. */
        float min_altitude = 0.0f;
        float max_altitude = 4000.0f;
    };

    GeoRegionNode();
    explicit GeoRegionNode(const GeoRegionSettings& settings);

    void set_settings(const GeoRegionSettings& settings) { m_settings = settings; }
    const GeoRegionSettings& get_settings() const { return m_settings; }
    void serialize_settings(QJsonObject& out) const override;
    void deserialize_settings(const QJsonObject& in) override;

public slots:
    void run_impl() override;

private:
    GeoRegionSettings m_settings;
    radix::geometry::Aabb<3, double> m_output_region;
};

} // namespace webgpu_compute::nodes
