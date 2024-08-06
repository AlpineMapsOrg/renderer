/*****************************************************************************
 * AlpineMaps.org Renderer
 * Copyright (C) 2024 Jakob Maier
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

#pragma once

#include <QDateTime>
#include <QString>
#include <QXmlStreamReader>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace nucleus::track {

// https://www.topografix.com/gpx.asp
// https://en.wikipedia.org/wiki/GPS_Exchange_Format
// TODO: handle waypoint and route

struct Point {
    double latitude = 0;
    double longitude = 0;
    double elevation = 0;
    QDateTime timestamp {};
};

using Segment = std::vector<Point>;
using Type = std::vector<Segment>;

struct Gpx {
    Type track;
    void add_new_segment() { track.push_back(Segment()); }
    void add_new_point(const Point& point)
    {
        if (track.empty()) {
            add_new_segment();
        }

        track.back().push_back(point);
    }

    Point& last_point()
    {
        if (track.empty()) {
            add_new_segment();
        }

        if (track.back().empty()) {
            add_new_point(Point());
        }

        return track.back().back();
    }
};

std::unique_ptr<Gpx> parse(const QString& path);

std::unique_ptr<Gpx> parse(QXmlStreamReader&);

std::vector<glm::vec4> to_world_points(const Gpx& gpx);

std::vector<glm::vec4> to_world_points(const track::Segment& segment);

// for rendering with GL_TRIANGLE_STRIP
std::vector<glm::vec3> triangle_strip_ribbon(const std::vector<glm::vec3>& points, float width);

// for rendering with GL_TRIANGLES
std::vector<glm::vec3> triangles_ribbon(const std::vector<glm::vec4>& points, float width, int index_offset = 0);

std::vector<unsigned> ribbon_indices(unsigned point_count);

void apply_gaussian_filter(std::vector<glm::vec4>& points, float sigma = 1.0f);

void reduce_point_count(std::vector<glm::vec4>& points, float threshold);

} // namespace nucleus::track
