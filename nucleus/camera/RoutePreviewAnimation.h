/*****************************************************************************
 * AlpineMaps.org Renderer
 * Copyright (C) 2026 Markus Rampp

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

#include "AnimationStyle.h"
#include <glm/glm.hpp>
#include <functional>
#include <vector>

namespace nucleus::camera {

// camera animation that descends along a route polyline, slightly above the
// terrain. the user can pause, scrub (seek), rotate around the current
// position and change the height above the terrain while the preview runs.
// the animation never terminates itself; it is removed explicitly via
// Controller::stop_route_preview
class RoutePreviewAnimation : public AnimationStyle {
public:
    // seconds from an arbitrary monotonic origin; the default clock is the
    // system steady clock
    using Clock = std::function<double()>;
    // world-space polyline (web-mercator metres, z = altitude in true metres
    // as delivered by srs::lat_long_alt_to_world)
    explicit RoutePreviewAnimation(const std::vector<glm::dvec3>& world_points);
    // test seam: drive time deterministically instead of the real clock
    RoutePreviewAnimation(const std::vector<glm::dvec3>& world_points, Clock clock);

    std::optional<Definition> update(Definition camera, AbstractDepthTester* depth_tester) override;

    // rotate the camera around the current position (drag)
    void orbit(double yaw_degrees, double pitch_degrees);
    void change_height(double delta_metres);
    void pause(bool paused);
    void set_dragging(bool dragging);
    void seek(double fraction); // 0 = route start, 1 = route end
    void set_speed(double metres_per_second);
    [[nodiscard]] double speed() const { return m_speed; }
    [[nodiscard]] double progress_fraction() const;
    // reached_end counts as paused, so the ui shows the play button
    [[nodiscard]] bool paused() const { return m_paused || m_dragging || m_reached_end; }

private:
    // position on the route at the given arclength
    glm::dvec3 route_point(double distance) const;
    double route_length() const { return m_cumulative_length.empty() ? 0.0 : m_cumulative_length.back(); }

    std::vector<glm::dvec3> m_points;
    std::vector<double> m_cumulative_length;

    Clock m_clock;
    double m_last_frame_time = 0; // each update consumes the gap to the previous one
    double m_progress = 0;     // metres along the route
    double m_yaw_offset = 0;   // user rotation around the current position
    double m_pitch_offset = 0;
    double m_height_offset = 30.0; // metres above the route point
    bool m_paused = false;
    bool m_dragging = false;
    bool m_reached_end = false;
    double m_speed = 24.0; // metres per second, default = "normal"

    static constexpr double k_lookahead_seconds = 3.0;
    static constexpr double k_min_height = 10.0;
    static constexpr double k_max_height = 500.0;
};

} // namespace nucleus::camera
