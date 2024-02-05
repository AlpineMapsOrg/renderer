#pragma once

#include <QString>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace nucleus {

// https://www.topografix.com/gpx.asp
// https://en.wikipedia.org/wiki/GPS_Exchange_Format
// TODO: handle waypoint and route
namespace gpx {

    using TrackPoint = glm::dvec3; // TODO: time?
    using TrackSegment = std::vector<TrackPoint>;
    using TrackType = std::vector<TrackSegment>;

    struct Gpx {
        TrackType track;
    };

    std::unique_ptr<Gpx> parse(const QString& path);

} // namespace gpx

std::vector<glm::vec3> to_world_points(const gpx::Gpx& gpx);

// for rendering with GL_TRIANGLE_STRIP
std::vector<glm::vec3> to_world_ribbon(const std::vector<glm::vec3>& points, float width);

// for rendering with GL_TRIANGLES
std::vector<glm::vec3> to_triangle_ribbon(const std::vector<glm::vec3>& points, float width);

std::vector<glm::vec3> to_world_ribbon_with_normals(const std::vector<glm::vec3>& points, float width);

void apply_gaussian_filter(std::vector<glm::vec3>& points, float sigma = 1.0f);

} // namespace nucleus
