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

#include "GeoRegionNode.h"

#include <QDebug>
#include <nucleus/srs.h>

namespace webgpu_compute::nodes {

GeoRegionNode::GeoRegionNode()
    : GeoRegionNode(GeoRegionSettings())
{
}

GeoRegionNode::GeoRegionNode(const GeoRegionSettings& settings)
    : Node({},
          {
              OutputSocket(*this, "region", data_type<const radix::geometry::Aabb<3, double>*>(), [this]() { return &m_output_region; }),
          })
    , m_settings { settings }
    , m_output_region { glm::dvec3(0.0), glm::dvec3(0.0) }
{
}

void GeoRegionNode::run_impl()
{
    const glm::dvec2 centre = nucleus::srs::lat_long_to_world(glm::dvec2(m_settings.center_latitude, m_settings.center_longitude));

    // Web Mercator is conformal but not equal-area: one metre of easting/northing in
    // projected units is 1/cos(latitude) real metres. Scaling by that keeps the requested
    // extent honest on the ground rather than only in projection space.
    const double latitude_scale = 1.0 / std::max(std::cos(glm::radians(m_settings.center_latitude)), 1e-6);
    const double half = 0.5 * double(std::max(m_settings.extent, 1.0f)) * latitude_scale;

    m_output_region = { glm::dvec3(centre.x - half, centre.y - half, double(m_settings.min_altitude)),
        glm::dvec3(centre.x + half, centre.y + half, double(m_settings.max_altitude)) };

    qInfo().nospace() << "GeoRegionNode: region " << m_settings.extent << " m around " << m_settings.center_latitude << ", "
                      << m_settings.center_longitude;

    complete_run();
}

void GeoRegionNode::serialize_settings(QJsonObject& out) const
{
    out["center_latitude"] = m_settings.center_latitude;
    out["center_longitude"] = m_settings.center_longitude;
    out["extent"] = m_settings.extent;
    out["min_altitude"] = m_settings.min_altitude;
    out["max_altitude"] = m_settings.max_altitude;
}

void GeoRegionNode::deserialize_settings(const QJsonObject& in)
{
    if (in.contains("center_latitude"))
        m_settings.center_latitude = in["center_latitude"].toDouble(m_settings.center_latitude);
    if (in.contains("center_longitude"))
        m_settings.center_longitude = in["center_longitude"].toDouble(m_settings.center_longitude);
    if (in.contains("extent"))
        m_settings.extent = float(in["extent"].toDouble(double(m_settings.extent)));
    if (in.contains("min_altitude"))
        m_settings.min_altitude = float(in["min_altitude"].toDouble(double(m_settings.min_altitude)));
    if (in.contains("max_altitude"))
        m_settings.max_altitude = float(in["max_altitude"].toDouble(double(m_settings.max_altitude)));
}

} // namespace webgpu_compute::nodes
