/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include <QMetaType>
#include <QPoint>
#include <glm/glm.hpp>
#include <vector>

#ifdef QT_GUI_LIB
#include <QtGui/QMouseEvent>
#include <QtGui/QTouchEvent>
#include <QtGui/QWheelEvent>
#endif

namespace nucleus::event_parameter {

// compatible to QEventPoint::State
enum TouchPointState {
    TouchPointUnknownState = 0x00,
    TouchPointPressed = 0x01,
    TouchPointMoved = 0x02,
    TouchPointStationary = 0x04,
    TouchPointReleased = 0x08
};

// loosely based on QEventPoint
struct EventPoint {
    TouchPointState state;
    glm::vec2 position = glm::vec2(0);
    glm::vec2 last_position = glm::vec2(0);
    glm::vec2 press_position = glm::vec2(0);
};

struct Touch {
    bool is_begin_event = false;
    bool is_end_event = false;
    bool is_update_event = false;
    std::vector<EventPoint> points;
};

struct Mouse {
    bool is_begin_event = false;
    bool is_end_event = false;
    bool is_update_event = false;
    Qt::MouseButton button = Qt::NoButton; // currently unused, but should contain the button that triggered the event.
    Qt::MouseButtons buttons;
    EventPoint point;
};

struct Wheel {
    bool is_begin_event = false;
    bool is_end_event = false;
    bool is_update_event = false;
    QPoint angle_delta;
    EventPoint point;
};
} // namespace nucleus::event_parameter
Q_DECLARE_METATYPE(nucleus::event_parameter::Touch)
Q_DECLARE_METATYPE(nucleus::event_parameter::Mouse)
Q_DECLARE_METATYPE(nucleus::event_parameter::Wheel)

#ifdef QT_GUI_LIB
namespace nucleus::event_parameter {
inline EventPoint make(const QEventPoint& e)
{
    EventPoint point;
    point.state = static_cast<TouchPointState>(e.state());
    point.position = glm::vec2(e.position().x(), e.position().y());
    point.last_position = glm::vec2(e.lastPosition().x(), e.lastPosition().y());
    point.press_position = glm::vec2(e.pressPosition().x(), e.pressPosition().y());
    return point;
}

inline Touch make(QTouchEvent* e)
{
    Touch touch;
    touch.is_begin_event = e->isBeginEvent();
    touch.is_end_event = e->isEndEvent();
    touch.is_update_event = e->isUpdateEvent();
    for (const auto& point : e->points()) {
        touch.points.push_back(make(point));
    }
    return touch;
}

inline Mouse make(QMouseEvent* e)
{
    Mouse mouse;
    mouse.is_begin_event = e->isBeginEvent();
    mouse.is_end_event = e->isEndEvent();
    mouse.is_update_event = e->isUpdateEvent();
    mouse.button = e->button();
    mouse.buttons = e->buttons();
    mouse.point = make(e->points().front());
    return mouse;
}

inline Wheel make(QWheelEvent* e)
{
    Wheel wheel;
    wheel.is_begin_event = e->isBeginEvent();
    wheel.is_end_event = e->isEndEvent();
    wheel.is_update_event = e->isUpdateEvent();
    wheel.angle_delta = e->angleDelta();
    wheel.point = make(e->points().front());
    return wheel;
}
} // namespace nucleus::event_parameter
#endif
