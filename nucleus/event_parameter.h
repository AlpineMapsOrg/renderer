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
#include <QTouchEvent>

namespace nucleus::event_parameter {

struct Touch {
    bool is_begin_event = false;
    bool is_end_event = false;
    QList<QEventPoint> points;
};
}
Q_DECLARE_METATYPE(nucleus::event_parameter::Touch)

namespace nucleus::event_parameter {
inline Touch make(const QTouchEvent* event)
{
    //    static int id = qRegisterMetaType<::nucleus::event_parameter::Touch>();
    return { event->isBeginEvent(), event->isEndEvent(), event->points() };
}
}
