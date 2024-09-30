/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#include "TimerFrontendObject.h"

#include <QDebug>

int TimerFrontendObject::timer_color_index = 0;

void TimerFrontendObject::add_measurement(float value, int frame) {
    m_quick_average = m_quick_average * m_old_weight + value * m_new_weight;
    m_measurements.append(QVector3D((float)frame, value, m_quick_average));
    if (m_measurements.size() > m_queue_size) m_measurements.removeFirst();
}

float TimerFrontendObject::get_last_measurement() {
    return m_measurements[m_measurements.length() - 1].y();
}

float TimerFrontendObject::get_average() {
    float sum = 0.0f;
    for (int i=0; i<m_measurements.count(); i++) {
        sum += m_measurements[i].y();
    }
    return sum / m_measurements.size();
}

TimerFrontendObject::TimerFrontendObject(QObject* parent, const QString& name, const QString& group, const int queue_size, const float average_weight, const float first_value)
    : QObject(parent)
    , m_name(name)
    , m_group(group)
    , m_queue_size(queue_size)
{
    m_color = timer_colors[timer_color_index++ % (sizeof(timer_colors) / sizeof(timer_colors[0]))];
    m_new_weight = average_weight;
    m_old_weight = 1.0f - m_new_weight;
    m_quick_average = first_value;
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    qDebug() << "TimerFrontendObject(name=" << m_name << ")";
#endif
}

TimerFrontendObject::~TimerFrontendObject() {
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    qDebug() << "~TimerFrontendObject(name=" << m_name << ")";
#endif
}
