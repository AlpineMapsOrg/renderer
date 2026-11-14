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

#include "PlannerManager.h"

namespace nucleus::planner {

PlannerManager::PlannerManager(QObject* parent)
    : QObject(parent)
{
}

void PlannerManager::set_edit_mode(bool edit_mode)
{
    m_edit_mode = edit_mode;
    if (!edit_mode) {
        m_in_click = false;
    }
}

void PlannerManager::mouse_press_event(const event_parameter::Mouse& e)
{
    if (!m_edit_mode) {
        return;
    }
    if (e.buttons & Qt::LeftButton) {
        start_click_event(e.point.position);
    }
}

void PlannerManager::mouse_release_event(const event_parameter::Mouse& e)
{
    if (m_in_click) {
        end_click_event(e.point.position);
    }
}

void PlannerManager::mouse_move_event(const event_parameter::Mouse& e)
{
    if (m_in_click && glm::length(e.point.position - m_press_position) > s_valid_click_distance) {
        m_in_click = false;
    }
}

void PlannerManager::touch_event(const event_parameter::Touch& e)
{
    if (!m_edit_mode) {
        return;
    }

    if (m_in_click) {
        if (e.points.size() > 1) {
            // became a multi touch gesture -> no longer a click
            m_in_click = false;
        } else if (e.points.size() == 1) {
            const event_parameter::EventPoint& point = e.points[0];
            if (point.state == event_parameter::TouchPointMoved
                && glm::length(point.position - m_press_position) > s_valid_click_distance) {
                m_in_click = false;
            } else if (point.state == event_parameter::TouchPointReleased) {
                end_click_event(point.position);
            }
        }
    } else if (e.points.size() == 1 && e.points[0].state == event_parameter::TouchPointPressed) {
        start_click_event(e.points[0].position);
    }
}

void PlannerManager::start_click_event(const glm::vec2& position)
{
    m_in_click = true;
    m_press_position = position;
}

void PlannerManager::end_click_event(const glm::vec2& position)
{
    m_in_click = false;
    if (glm::length(position - m_press_position) <= s_valid_click_distance) {
        emit click_detected({ position.x, position.y });
    }
}

} // namespace nucleus::planner
