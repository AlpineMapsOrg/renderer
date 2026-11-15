/*****************************************************************************
 * AlpineMaps.org Renderer
 * Copyright (C) 2024 Lucas Dworschak
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

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <glm/glm.hpp>

#include <nucleus/event_parameter.h>

namespace nucleus::planner {

// Detects clicks (press + release without much movement) on the map, without
// interfering with camera navigation (drags are still handled by the camera).
// While a button is pressed it additionally reports the raw press / move /
// release gesture (move events throttled), which the route planner uses for
// its hold-to-drag point editing. All signals are only emitted while edit
// mode is enabled.
class PlannerManager : public QObject {
    Q_OBJECT
public:
    explicit PlannerManager(QObject* parent = nullptr);

public slots:
    void set_edit_mode(bool edit_mode);
    void mouse_press_event(const event_parameter::Mouse& e);
    void mouse_release_event(const event_parameter::Mouse& e);
    void mouse_move_event(const event_parameter::Mouse& e);
    void touch_event(const event_parameter::Touch& e);

signals:
    void click_detected(const QPointF& screen_position);
    void press_detected(const QPointF& screen_position);
    void move_detected(const QPointF& screen_position);
    void release_detected();

private:
    void start_click_event(const glm::vec2& position);
    void end_click_event(const glm::vec2& position);
    void emit_move_throttled(const glm::vec2& position);

    bool m_edit_mode = false;
    bool m_in_click = false;
    bool m_pressed = false;
    glm::vec2 m_press_position = glm::vec2(0);
    QElapsedTimer m_move_throttle;

    // how far the pointer may travel for the gesture to still count as a click
    static constexpr float s_valid_click_distance = 30.0f;
    // drag-move events trigger a depth readback each; keep the rate bounded
    static constexpr qint64 s_move_emit_interval_ms = 50;
};

} // namespace nucleus::planner
