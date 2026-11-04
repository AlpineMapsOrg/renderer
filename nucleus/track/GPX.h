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

#include "nucleus/srs.h"
#include <QDateTime>
#include <QString>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <algorithm>
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
using BoundingBox = radix::geometry::Aabb<3, double>;

struct Gpx {
    Type track;
    QString name;
    QString description;
    QDateTime creationDateTime;

    void add_new_segment() { track.push_back(Segment()); }
    void add_new_point(const Point& point)
    {
        if (track.empty()) {
            add_new_segment();
        }

        track.back().push_back(point);
    }

    void delete_point(unsigned index)
    {
        if (track.empty() || index >= track.back().size()) {
            return;
        }
        track.back().erase(track.back().begin() + index);
    }

    void delete_last_point()
    {
        if (track.empty() || track.back().empty()) {
            return;
        }
        track.back().pop_back();
    }

    void insert_point(unsigned index, const Point& point)
    {
        if (track.empty()) {
            add_new_segment();
        }
        track.back().insert(track.back().begin() + std::min<size_t>(index, track.back().size()), point);
    }

    // moves a point within the last segment, returns the index it ended up at
    unsigned move_point(unsigned from, int steps)
    {
        if (track.empty()) {
            return from;
        }
        Segment& segment = track.back();
        const int target = static_cast<int>(from) + steps;
        if (from >= segment.size() || target < 0 || target >= static_cast<int>(segment.size())) {
            return from;
        }
        const Point point = segment[from];
        segment.erase(segment.begin() + from);
        segment.insert(segment.begin() + target, point);
        return static_cast<unsigned>(target);
    }

    void reverse()
    {
        std::reverse(track.begin(), track.end());
        for (Segment& segment : track) {
            std::reverse(segment.begin(), segment.end());
        }
    }

    void reset()
    {
        track.clear();
        name.clear();
        description.clear();
    }

    unsigned point_count() const
    {
        unsigned count = 0;
        for (const Segment& segment : track) {
            count += static_cast<unsigned>(segment.size());
        }
        return count;
    }

    double haversine_distance(const Point& a, const Point& b) const;

    // distance in metres, ascent/descent in metres (from point elevations)
    double distance = 0;
    double ascent = 0;
    double descent = 0;
    void recompute_stats();

    void save(const QString& file_path) const;
    void save(QXmlStreamWriter& xml) const;

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

BoundingBox compute_world_aabb(const Gpx& gpx);

// for rendering with GL_TRIANGLE_STRIP
std::vector<glm::vec3> triangle_strip_ribbon(const std::vector<glm::vec3>& points, float width);

// for rendering with GL_TRIANGLES
std::vector<glm::vec3> triangles_ribbon(const std::vector<glm::vec4>& points, float width, int index_offset = 0);

std::vector<unsigned> ribbon_indices(unsigned point_count);

void apply_gaussian_filter(std::vector<glm::vec4>& points, float sigma = 1.0f);

void reduce_point_count(std::vector<glm::vec4>& points, float threshold);

} // namespace nucleus::track
