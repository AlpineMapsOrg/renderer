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

#pragma once

#include "Node.h"

namespace webgpu_compute::nodes {

class GPXTrackNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(GPXTrackNode)

    struct GPXTrackNodeSettings {
        std::string file_path = ":/gpx/breite_ries.gpx";
        bool enable_caching = true;
    };

    GPXTrackNode();

    void set_settings(const GPXTrackNodeSettings& settings) { m_settings = settings; }
    const GPXTrackNodeSettings& get_settings() const { return m_settings; }
    void serialize_settings(QJsonObject& out) const override;
    void deserialize_settings(const QJsonObject& in) override;

public slots:
    void run_impl() override;

private:
    GPXTrackNodeSettings m_settings;
    radix::geometry::Aabb<3, double> m_output_region;

    std::string m_cached_path;
    bool m_has_cached = false;
};

} // namespace webgpu_compute::nodes
