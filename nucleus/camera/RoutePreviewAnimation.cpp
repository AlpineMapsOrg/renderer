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

#include "RoutePreviewAnimation.h"

#include <nucleus/camera/Definition.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace nucleus::camera {

namespace {
double steady_clock_seconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

RoutePreviewAnimation::RoutePreviewAnimation(const std::vector<glm::dvec3>& world_points)
    : RoutePreviewAnimation(world_points, &steady_clock_seconds)
{
}

RoutePreviewAnimation::RoutePreviewAnimation(const std::vector<glm::dvec3>& world_points, Clock clock)
    : m_points(world_points)
    , m_clock(std::move(clock))
{
    m_cumulative_length.reserve(m_points.size());
    double total = 0.0;
    for (size_t i = 0; i < m_points.size(); ++i) {
        if (i > 0) {
            total += glm::length(m_points[i] - m_points[i - 1]);
        }
        m_cumulative_length.push_back(total);
    }
    // the first frame advances by the time since the animation was created
    m_last_frame_time = m_clock();
}

glm::dvec3 RoutePreviewAnimation::route_point(double distance) const
{
    if (m_points.empty()) {
        return {};
    }
    distance = std::clamp(distance, 0.0, route_length());

    // find the segment containing the distance (linear scan is fine for the
    // few hundred points of a route; could be binary search if needed)
    auto upper = std::upper_bound(m_cumulative_length.begin(), m_cumulative_length.end(), distance);
    if (upper == m_cumulative_length.begin()) {
        return m_points.front();
    }
    if (upper == m_cumulative_length.end()) {
        return m_points.back();
    }
    const size_t index = size_t(upper - m_cumulative_length.begin());
    const double segment_start = m_cumulative_length[index - 1];
    const double segment_length = m_cumulative_length[index] - segment_start;
    const double t = segment_length > 0.0 ? (distance - segment_start) / segment_length : 0.0;
    return m_points[index - 1] + (m_points[index] - m_points[index - 1]) * t;
}

std::optional<Definition> RoutePreviewAnimation::update(Definition camera, AbstractDepthTester*)
{
    if (m_points.size() < 2) {
        return camera;
    }

    const double now = m_clock();
    const double dt = std::min(now - m_last_frame_time, 0.1); // seconds, clamp huge gaps
    m_last_frame_time = now;

    if (!m_paused && !m_dragging && !m_reached_end) {
        m_progress += m_speed * dt;
        if (m_progress >= route_length()) {
            m_progress = route_length();
            m_reached_end = true; // auto-pause at the end, the camera stays
        }
    }

    const double lookahead = m_speed * k_lookahead_seconds;
    const glm::dvec3 position = route_point(m_progress);
    const glm::dvec3 look_target = route_point(m_progress + lookahead);
    const glm::dvec3 camera_position = position + glm::dvec3(0.0, 0.0, m_height_offset);

    Definition preview(camera);
    // if the lookahead point equals the current point (route end), look
    // slightly ahead along the horizontal direction of the last segment
    glm::dvec3 target = look_target;
    if (glm::length(target - position) < 1.0) {
        const glm::dvec3 behind = route_point(std::max(0.0, m_progress - 20.0));
        glm::dvec3 horizontal_forward = position - behind;
        horizontal_forward.z = 0.0;
        if (glm::length(horizontal_forward) > 1.0) {
            target = position + glm::normalize(horizontal_forward) * 50.0;
        } else {
            target = position + glm::dvec3(50.0, 0.0, 0.0);
        }
    }
    preview.look_at(camera_position, target + glm::dvec3(0.0, 0.0, m_height_offset * 0.5));

    // apply the user's rotation around the current position (orientation
    // only, unclamped - the user may look however they like)
    if (std::abs(m_yaw_offset) > 0.001 || std::abs(m_pitch_offset) > 0.001) {
        preview.orbit(camera_position, { m_yaw_offset, m_pitch_offset });
    }

    return preview;
}

void RoutePreviewAnimation::orbit(double yaw_degrees, double pitch_degrees)
{
    m_yaw_offset += yaw_degrees;
    m_pitch_offset += pitch_degrees;
}

void RoutePreviewAnimation::change_height(double delta_metres)
{
    m_height_offset = std::clamp(m_height_offset + delta_metres, k_min_height, k_max_height);
}

void RoutePreviewAnimation::pause(bool paused)
{
    m_paused = paused;
    if (!paused) {
        // pressing play at the route end restarts the fly-through from the top
        if (m_reached_end) {
            m_progress = 0.0;
            m_yaw_offset = 0.0;
            m_pitch_offset = 0.0;
        }
        // do not jump forward by the paused time
        m_last_frame_time = m_clock();
        m_reached_end = false;
    }
}

void RoutePreviewAnimation::set_speed(double metres_per_second)
{
    m_speed = std::clamp(metres_per_second, 2.0, 120.0);
}

void RoutePreviewAnimation::set_dragging(bool dragging)
{
    m_dragging = dragging;
    if (!dragging) {
        m_last_frame_time = m_clock();
    }
}

void RoutePreviewAnimation::seek(double fraction)
{
    m_progress = std::clamp(fraction, 0.0, 1.0) * route_length();
    m_reached_end = false;
    m_last_frame_time = m_clock();
}

double RoutePreviewAnimation::progress_fraction() const
{
    const double length = route_length();
    return length > 0.0 ? m_progress / length : 0.0;
}

} // namespace nucleus::camera
