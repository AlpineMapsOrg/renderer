/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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
#include <QMouseEvent>
#include <QTouchEvent>

namespace nucleus::event_parameter {

struct Touch {
    bool is_begin_event = false;
    bool is_end_event = false;
    bool is_update_event = false;
    QList<QEventPoint> points;
    QEventPoint::States states;
};

struct Mouse {
    bool is_begin_event = false;
    bool is_end_event = false;
    bool is_update_event = false;
    Qt::MouseButton button = Qt::NoButton;
    Qt::MouseButtons buttons;
    QEventPoint point;
};

struct Wheel {
    bool is_begin_event = false;
    bool is_end_event = false;
    bool is_update_event = false;
    QPoint angle_delta;
    QEventPoint point;
};
}
Q_DECLARE_METATYPE(nucleus::event_parameter::Touch)

namespace nucleus::event_parameter {
inline Touch make(const QTouchEvent* event)
{
    //    static int id = qRegisterMetaType<::nucleus::event_parameter::Touch>();
    return { event->isBeginEvent(), event->isEndEvent(), event->isUpdateEvent(),
        event->points(), event->touchPointStates() };
}
inline Mouse make(const QMouseEvent* event)
{
    assert(event->points().size() == 1);

    return { event->isBeginEvent(), event->isEndEvent(), event->isUpdateEvent(),
        event->button(), event->buttons(), event->points().front() };
}
inline Wheel make(const QWheelEvent* event)
{
    assert(event->points().size() == 1);

    return { event->isBeginEvent(), event->isEndEvent(), event->isUpdateEvent(),
        event->angleDelta(), event->points().front() };
}
}
