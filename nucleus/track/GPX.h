
/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2024 Jakob Maier
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

#include <QDateTime>
#include <QString>
#include <QXmlStreamReader>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace nucleus {

// https://www.topografix.com/gpx.asp
// https://en.wikipedia.org/wiki/GPS_Exchange_Format
// TODO: handle waypoint and route
namespace gpx {

    struct TrackPoint {
        double latitude = 0;
        double longitude = 0;
        double elevation = 0;
        QDateTime timestamp {};
    };

    using TrackSegment = std::vector<TrackPoint>;
    using TrackType = std::vector<TrackSegment>;

    struct Gpx {
        TrackType track;
        void add_new_segment() { track.push_back(TrackSegment()); }
        void add_new_point(const TrackPoint& point)
        {
            if (track.empty()) {
                add_new_segment();
            }

            track.back().push_back(point);
        }

        TrackPoint& last_point()
        {
            if (track.empty()) {
                add_new_segment();
            }

            if (track.back().empty()) {
                add_new_point(TrackPoint());
            }

            return track.back().back();
        }
    };

    std::unique_ptr<Gpx> parse(const QString& path);

    std::unique_ptr<Gpx> parse(QXmlStreamReader&);

} // namespace gpx

std::vector<glm::vec4> to_world_points(const gpx::Gpx& gpx);

std::vector<glm::vec4> to_world_points(const gpx::TrackSegment& segment);

// for rendering with GL_TRIANGLE_STRIP
std::vector<glm::vec3> triangle_strip_ribbon(const std::vector<glm::vec3>& points, float width);

// for rendering with GL_TRIANGLES
std::vector<glm::vec3> triangles_ribbon(const std::vector<glm::vec4>& points, float width, int index_offset = 0);

std::vector<unsigned> ribbon_indices(unsigned point_count);

void apply_gaussian_filter(std::vector<glm::vec4>& points, float sigma = 1.0f);

void reduce_point_count(std::vector<glm::vec4>& points, float threshold);

} // namespace nucleus
