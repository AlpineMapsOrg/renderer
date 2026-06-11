/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
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

#include "GPXTrackNode.h"

#include "nucleus/track/GPX.h"
#include <QDebug>
#include <QString>

namespace webgpu_compute::nodes {

GPXTrackNode::GPXTrackNode()
    : Node({},
          {
              OutputSocket(*this, "region", data_type<const radix::geometry::Aabb<3, double>*>(), [this]() { return &m_output_region; }),
          })
{
}

void GPXTrackNode::run_impl()
{
    if (m_settings.enable_caching && m_has_cached && m_settings.file_path == m_cached_path) {
        complete_run();
        return;
    }

    std::unique_ptr<nucleus::track::Gpx> gpx = nucleus::track::parse(QString::fromStdString(m_settings.file_path));
    if (!gpx) {
        fail_run("could not parse GPX file: " + m_settings.file_path);
        return;
    }

    m_output_region = nucleus::track::compute_world_aabb(*gpx);
    m_cached_path = m_settings.file_path;
    m_has_cached = true;

    qDebug() << Qt::fixed << "gpx region=[(" << m_output_region.min.x << ", " << m_output_region.min.y << "), (" << m_output_region.max.x << ", "
             << m_output_region.max.y << ")]";

    complete_run();
}

} // namespace webgpu_compute::nodes
