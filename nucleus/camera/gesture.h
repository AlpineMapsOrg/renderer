/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Adam Celarek
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

#include <QDebug>
#include <QElapsedTimer>
#include <QString>
#include <glm/glm.hpp>
#include <memory>
#include <nucleus/event_parameter.h>
#include <optional>
#include <unordered_map>
#include <vector>

namespace nucleus::camera::gesture {

enum class Type { Pan, TapTapMove, Shove, PinchAndRotate };
enum class ValueName { Pivot, Pan, TapTapMoveAbsolute, TapTapMoveRelative, Shove, Pinch, Rotation };

struct Result {
    Type type;
    bool just_activated = false;
    std::unordered_map<ValueName, glm::vec2> values;
};

class Detector {
public:
    virtual ~Detector() = default;
    virtual std::optional<Result> analise(const event_parameter::Touch& t) = 0;
    virtual QString name() const = 0;
};

class Controller {
public:
    Controller(std::vector<std::unique_ptr<Detector>>&& detectors)
        : m_detectors(std::move(detectors))
    {
    }

    std::optional<Result> analise(const event_parameter::Touch& t, const glm::uvec2& viewport_size)
    {
        assert(std::is_sorted(t.points.begin(), t.points.end(), [](const auto& a, const auto& b) { return a.id < b.id; }));
        const auto filtered_touch = filter_sloppy_touches(t, viewport_size);

        if (m_active_detector) {
            auto gesture = m_active_detector->analise(filtered_touch);
            if (gesture) {
                return gesture;
            } else {
                qDebug() << "Detector ended:" << m_active_detector->name();
                m_active_detector = nullptr;
            }
        }

        if (!m_active_detector) {
            for (auto& detector : m_detectors) {
                auto gesture = detector->analise(filtered_touch);
                if (gesture) {
                    m_active_detector = detector.get();
                    qDebug() << "Detector began:" << m_active_detector->name();
                    return gesture;
                }
            }
        }
        return {};
    }

private:
    event_parameter::Touch filter_sloppy_touches(const event_parameter::Touch& t, const glm::uvec2& viewport_size) const
    {
        event_parameter::Touch filtered_touch = t;
        filtered_touch.points.erase(std::remove_if(filtered_touch.points.begin(),
                                        filtered_touch.points.end(),
                                        [this, viewport_size](const event_parameter::EventPoint& p) {
                                            return p.position.x < m_screen_edge_margin || p.position.y < m_screen_edge_margin
                                                || p.position.x > viewport_size.x - m_screen_edge_margin
                                                || p.position.y > viewport_size.y - m_screen_edge_margin;
                                        }),
            filtered_touch.points.end());
        return filtered_touch;
    }

    std::vector<std::unique_ptr<Detector>> m_detectors;
    Detector* m_active_detector = nullptr;
    const float m_screen_edge_margin = 10;
};

class PanDetector : public Detector {
public:
    PanDetector() { m_long_press_timer.start(); }

    QString name() const override { return "PanDetector"; }

    std::optional<Result> analise(const event_parameter::Touch& t) override
    {
        const auto& points = t.points;
        if (points.size() != 1 || points.front().state == event_parameter::TouchPointReleased) {
            m_active = false;
            return {};
        }

        const auto& point = t.points.front();
        if (point.state == event_parameter::TouchPointPressed) {
            m_long_press_timer.start();
            m_last_pos = point.press_position;
            return {};
        }

        bool just_activated = false;
        if (m_active == false) {
            if (m_long_press_timer.elapsed() > m_long_press_timeout)
                return {};
            if (glm::distance(point.position, m_last_pos) < m_move_threshold) {
                return {};
            } else {
                m_active = true;
                just_activated = true;
            }
        }

        Result g;
        g.type = Type::Pan;
        g.just_activated = just_activated;
        g.values[ValueName::Pivot] = point.position;
        g.values[ValueName::Pan] = point.position - m_last_pos;
        m_last_pos = point.position;
        return g;
    }

private:
    const float m_move_threshold = 25.0f;
    const unsigned m_long_press_timeout { 500 };
    bool m_active = false;
    glm::vec2 m_last_pos { 0.0f };
    QElapsedTimer m_long_press_timer;
};

class TapTapMoveDetector : public Detector {
public:
    TapTapMoveDetector() { m_second_tap_timer.start(); }

    QString name() const override { return "TapTapMoveDetector"; }

    std::optional<Result> analise(const event_parameter::Touch& t) override
    {
        const auto& points = t.points;
        if (points.size() != 1 || points.front().state == event_parameter::TouchPointReleased) {
            m_active = false;
            return {};
        }

        const auto& point = t.points.front();

        if (m_active) {
            Result g;
            g.type = Type::TapTapMove;
            g.values[ValueName::Pivot] = m_first_tap_pos;
            g.values[ValueName::TapTapMoveAbsolute] = point.position - m_second_tap_start_pos;
            g.values[ValueName::TapTapMoveRelative] = point.position - point.last_position;
            return g;
        }

        if (point.state == event_parameter::TouchPointPressed && m_second_tap_timer.elapsed() < m_second_tap_timeout
            && glm::distance(point.position, m_first_tap_pos) < m_distance_threshold) {
            m_active = true;
            m_second_tap_start_pos = point.position;

            Result g;
            g.type = Type::TapTapMove;
            g.just_activated = true;
            g.values[ValueName::Pivot] = m_first_tap_pos;
            g.values[ValueName::TapTapMoveAbsolute] = {};
            g.values[ValueName::TapTapMoveRelative] = {};
            return g;
        }

        if (point.state == event_parameter::TouchPointPressed) {
            m_second_tap_timer.start();
            m_first_tap_pos = point.press_position;
        }
        return {};
    }

private:
    const float m_distance_threshold = 50.0f;
    const unsigned m_second_tap_timeout { 300 };
    QElapsedTimer m_second_tap_timer;
    glm::vec2 m_first_tap_pos { 0.f };
    glm::vec2 m_second_tap_start_pos { 0.f };
    glm::vec2 m_last_pos { 0.f };
    bool m_active = false;
};

class ShoveDetector : public Detector {
public:
    enum class Type { Horizontal, Vertical, Both };
    explicit ShoveDetector(Type type)
        : m_type(type)
    {
    }

    QString name() const override { return "ShoveDetector"; }
    void reset()
    {
        m_active_shove = ActiveShoveType::None;
        m_active = false;
        m_touch_id1 = -1;
        m_touch_id2 = -1;
    }

    std::optional<Result> analise(const event_parameter::Touch& t) override
    {
        const auto& points = t.points;
        if (points.size() != 2) {
            reset();
            return {};
        }

        const auto p1 = points[0];
        const auto p2 = points[1];
        const auto mean = 0.5f * (p1.position + p2.position);
        const auto delta_positions = abs(p1.position - p2.position);
        const auto degs_to_horizontal = glm::degrees(std::atan(delta_positions.y / delta_positions.x));
        const auto degs_to_vertical = glm::degrees(std::atan(delta_positions.x / delta_positions.y));
        const auto angle = glm::degrees(std::atan2(delta_positions.y, delta_positions.x));
        const auto dist = glm::length(delta_positions);
        // qDebug() << "degs_to_horizontal: " << degs_to_horizontal << ", degs_to_vertical: " << degs_to_vertical;

        if (m_active_shove == ActiveShoveType::None && (p1.state == event_parameter::TouchPointPressed || p2.state == event_parameter::TouchPointPressed)) {
            if (degs_to_horizontal < m_alignment_threshold && m_type != Type::Horizontal) {
                m_pivot = mean;
                m_active_shove = ActiveShoveType::Vertical;
                m_start_angle = angle;
                m_start_distance = dist;
                qDebug() << "degs_to_horizontal " << degs_to_horizontal << " < m_alignment_threshold => type = Vertical";
            }
            if (degs_to_vertical < m_alignment_threshold && m_type != Type::Vertical) {
                m_pivot = mean;
                m_active_shove = ActiveShoveType::Horizontal;
                m_start_angle = angle;
                m_start_distance = dist;
                qDebug() << "degs_to_vertical " << degs_to_vertical << " < m_alignment_threshold => type = Horizontal";
            }
            m_last_mean = m_pivot;
            return {};
        }

        if (m_active_shove != ActiveShoveType::None && m_active == false
            && (std::abs(angle - m_start_angle) > m_synchronicity_degree_threshold
                || std::abs(dist / m_start_distance - 1.f) > m_synchronicity_distance_scale_threshold)) {
            reset();
            if (std::abs(angle - m_start_angle) > m_synchronicity_degree_threshold)
                qDebug() << "reset based on std::abs(angle - m_start_angle) > m_synchronicity_degree_threshold (" << std::abs(angle - m_start_angle) << ")";

            if (std::abs(dist / m_start_distance - 1.f) > m_synchronicity_distance_scale_threshold)
                qDebug() << "reset based on std::abs(dist / m_start_distance - 1.f) > m_synchronicity_distance_scale_threshold ("
                         << std::abs(dist / m_start_distance - 1.f) << ")";
            return {};
        }

        Result g;
        g.type = gesture::Type::Shove;
        g.values[ValueName::Pivot] = m_pivot;

        if (m_active_shove != ActiveShoveType::None && m_active == false) {
            const auto delta_centre = mean - m_pivot;
            m_last_mean = mean;
            g.just_activated = true;
            if (std::abs(delta_centre.y) > m_perpendicular_move_threshold && degs_to_horizontal < m_alignment_threshold
                && m_active_shove == ActiveShoveType::Vertical) {
                g.values[ValueName::Shove] = { 0, delta_centre.y };
                m_active = true;
                return g;
            }
            if (std::abs(delta_centre.x) > m_perpendicular_move_threshold && degs_to_vertical < m_alignment_threshold
                && m_active_shove == ActiveShoveType::Horizontal) {
                g.values[ValueName::Shove] = { delta_centre.x, 0 };
                m_active = true;
                return g;
            }
            qDebug() << "not activated based on m_perpendicular_move_threshold (" << std::abs(delta_centre.y) << ") or degs_to_horizontal";
            return {};
        }

        if (m_active) {
            const auto delta = mean - m_last_mean;
            m_last_mean = mean;
            if (m_active_shove == ActiveShoveType::Vertical) {
                g.values[ValueName::Shove] = { 0, delta.y };
            }
            if (m_active_shove == ActiveShoveType::Horizontal) {
                g.values[ValueName::Shove] = { delta.x, 0 };
            }
            return g;
        }
        return {};
    }

private:
    const float m_alignment_threshold = 40.f; // degrees
    const float m_synchronicity_degree_threshold = 4.f;
    const float m_synchronicity_distance_scale_threshold = 0.08f;

    const float m_perpendicular_move_threshold = 10.0f;

    enum class ActiveShoveType { None, Horizontal, Vertical };
    Type m_type;
    ActiveShoveType m_active_shove = ActiveShoveType::None;
    bool m_active = false;
    glm::vec2 m_pivot = {};
    glm::vec2 m_last_mean = {};
    float m_start_angle = 0;
    float m_start_distance = 0;
    int m_touch_id1 = -1, m_touch_id2 = -1;
};

class PinchAndRotateDetector : public Detector {
public:
    enum class Mode { PinchOnly, RotateOnly, PinchAndRotate };
    explicit PinchAndRotateDetector(Mode mode)
        : m_mode(mode)
    {
    }

    QString name() const override { return "PinchAndRotateDetector"; }
    void reset()
    {
        m_touch_ids.clear();
        m_active_pinch = false;
        m_active_rotate = false;
    }

    static std::tuple<glm::vec2, float, std::unordered_map<int, float>> compute_pivot_span_angles(const std::vector<event_parameter::EventPoint>& points)
    {
        glm::vec2 pivot = {};
        for (const auto& p : points) {
            pivot += p.position;
        }
        pivot /= points.size();

        float span = 0;
        std::unordered_map<int, float> angles = {};
        angles.reserve(points.size());

        for (const auto& p : points) {
            span += glm::distance(p.position, pivot);
            const auto vec = p.position - pivot;
            angles[p.id] = atan2(vec.y, vec.x);
        }

        span = 2.f * span / points.size();

        return { pivot, span, angles };
    }

    std::optional<Result> analise(const event_parameter::Touch& t) override
    {
        const auto& points = t.points;
        if (points.size() < 2) {
            reset();
            return {};
        }

        std::vector<int> current_ids;
        for (const auto& p : points)
            current_ids.push_back(p.id);

        if (current_ids != m_touch_ids) {
            reset();
            m_touch_ids = current_ids;

            // Initial state for new touch set
            std::tie(m_last_centre, m_last_span, m_last_angles) = compute_pivot_span_angles(points);

            return {}; // Half-activated, waiting for movement
        }

        // --- Calculations ---
        const auto [current_centre, current_span, current_angles] = compute_pivot_span_angles(points);

        float pinch_val = (m_last_span > 1.f) ? current_span / m_last_span : 1.0f;
        // qDebug() << "current_span: " << current_span << ", last_span: " << m_last_span << ", pinch_val:" << pinch_val;

        float rotation_val = 0;
        for (const auto id : m_touch_ids) {
            assert(m_last_angles.contains(id));
            assert(current_angles.contains(id));
            const auto last_angle = m_last_angles.at(id);
            const auto current_angle = current_angles.at(id);
            auto diff = current_angle - last_angle;
            if (diff > M_PI)
                diff -= 2 * M_PI;
            if (diff < -M_PI)
                diff += 2 * M_PI;
            rotation_val += diff;
        }
        rotation_val = glm::degrees(rotation_val / m_touch_ids.size());
        assert(std::abs(rotation_val) < 180);

        // --- Activation ---
        Result g;
        g.type = Type::PinchAndRotate;
        if (!m_active_pinch && m_mode != Mode::RotateOnly) {
            if (std::abs(pinch_val - 1.0f) > m_pinch_threshold) {
                m_active_pinch = true;
                g.just_activated = true;
            }
        }
        if (!m_active_rotate && m_mode != Mode::PinchOnly) {
            if (std::abs(rotation_val) > m_rotate_threshold) {
                m_active_rotate = true;
                g.just_activated = true;
            }
        }

        if (!m_active_pinch && !m_active_rotate) {
            return {}; // Not active yet
        }

        // --- Result Reporting ---
        g.values[ValueName::Pivot] = current_centre;
        g.values[ValueName::Pan] = current_centre - m_last_centre;

        if (m_active_pinch)
            g.values[ValueName::Pinch] = { pinch_val, 0 };
        if (m_active_rotate)
            g.values[ValueName::Rotation] = { rotation_val, 0 };

        // Update last values only after activation and use
        m_last_centre = current_centre;
        if (m_active_pinch)
            m_last_span = current_span;
        if (m_active_rotate) {
            m_last_angles = std::move(current_angles);
        }

        return g;
    }

private:
    const float m_pinch_threshold = 0.08f;
    const float m_rotate_threshold = 4.0f;

    Mode m_mode;
    bool m_active_pinch = false;
    bool m_active_rotate = false;

    std::vector<int> m_touch_ids;
    glm::vec2 m_last_centre { 0.f };
    float m_last_span = 0.f;
    std::unordered_map<int, float> m_last_angles;
};

// idea: 3 touch for orbit?

} // namespace nucleus::camera::gesture
