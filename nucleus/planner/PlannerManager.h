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

#include <QObject>
#include <QPointF>
#include <glm/glm.hpp>

#include <nucleus/event_parameter.h>

namespace nucleus::planner {

// Detects clicks (press + release without much movement) on the map, without
// interfering with camera navigation (drags are still handled by the camera).
// Clicks are only reported while edit mode is enabled.
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

private:
    void start_click_event(const glm::vec2& position);
    void end_click_event(const glm::vec2& position);

    bool m_edit_mode = false;
    bool m_in_click = false;
    glm::vec2 m_press_position = glm::vec2(0);

    // how far the pointer may travel for the gesture to still count as a click
    static constexpr float s_valid_click_distance = 30.0f;
};

} // namespace nucleus::planner
