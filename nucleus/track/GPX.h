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
#include <optional>
#include <string>
#include <utility>
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

    // deletes the point at the given flattened index (counting across all
    // segments, as the ui lists them); drops the segment when it empties
    void delete_point(unsigned index)
    {
        const auto location = locate_point(index);
        if (!location) {
            return;
        }
        Segment& segment = track[location->first];
        segment.erase(segment.begin() + location->second);
        if (segment.empty()) {
            track.erase(track.begin() + location->first);
        }
    }

    // removes the last point of the last non-empty segment (earlier edits or
    // an empty <trkseg/> from the parser may have left empty segments behind)
    // and drops the segment when it empties
    void delete_last_point()
    {
        while (!track.empty() && track.back().empty()) {
            track.pop_back();
        }
        if (track.empty()) {
            return;
        }
        track.back().pop_back();
        if (track.back().empty()) {
            track.pop_back();
        }
    }

    // inserts the point at the given flattened index (counting across all
    // segments; appends when the index equals the point count)
    void insert_point(unsigned index, const Point& point) { insert_at_flat(index, point); }

    // moves the point at the given flattened index to new coordinates
    void update_point(unsigned index, const Point& point)
    {
        const auto location = locate_point(index);
        if (!location) {
            return;
        }
        track[location->first][location->second] = point;
    }

    // moves the point at the given flattened index by steps positions within
    // the flattened sequence, across segment boundaries if needed. returns
    // the flattened index the point ended up at, or `from` if the move was a
    // no-op (invalid index or boundary)
    unsigned move_point(unsigned from, int steps)
    {
        const unsigned total = point_count();
        if (steps == 0 || from >= total) {
            return from;
        }
        const long long target = static_cast<long long>(from) + steps;
        if (target < 0 || target >= static_cast<long long>(total)) {
            return from;
        }
        const auto source = locate_point(from);
        if (!source) {
            return from;
        }
        Point point = track[source->first][source->second];
        track[source->first].erase(track[source->first].begin() + source->second);
        if (track[source->first].empty()) {
            track.erase(track.begin() + source->first);
        }
        // the point must end up at the target's flat position in the final
        // sequence; inserting at that position (before the point currently
        // there) does exactly that, source segment purge included
        insert_at_flat(unsigned(target), point);
        return unsigned(target);
    }

    // merges all segments into one and drops empty ones. the route planner
    // edits one continuous leg, so imported multi-segment tracks are merged
    // into the single editable sequence on import
    void flatten_segments()
    {
        Segment merged;
        merged.reserve(point_count());
        for (const Segment& segment : track) {
            merged.insert(merged.end(), segment.begin(), segment.end());
        }
        track.clear();
        if (!merged.empty()) {
            track.push_back(std::move(merged));
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

    // writes gpx 1.1; false when the file could not be opened or written
    bool save(const QString& file_path) const;
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

private:
    // resolves a flattened point index (counting across all segments) to its
    // segment and position within it; empty segments hold no points.
    // nullopt when the index is out of range
    std::optional<std::pair<unsigned, unsigned>> locate_point(unsigned flat) const
    {
        unsigned count = 0;
        for (size_t s = 0; s < track.size(); ++s) {
            if (track[s].empty()) {
                continue;
            }
            if (flat < count + track[s].size()) {
                return std::pair<unsigned, unsigned>(unsigned(s), flat - count);
            }
            count += unsigned(track[s].size());
        }
        return std::nullopt;
    }

    // inserts the point so that it ends up at the given flattened index;
    // appends to the last segment when the index equals the point count
    void insert_at_flat(unsigned flat, const Point& point)
    {
        if (track.empty()) {
            add_new_segment();
        }
        unsigned count = 0;
        for (Segment& segment : track) {
            if (segment.empty()) {
                continue;
            }
            if (flat < count + segment.size()) {
                segment.insert(segment.begin() + (flat - count), point);
                return;
            }
            count += unsigned(segment.size());
        }
        track.back().push_back(point);
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
