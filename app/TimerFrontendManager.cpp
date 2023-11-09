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

#include "TimerFrontendManager.h"

#include <QMap>
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
    :QObject(parent), m_queue_size(queue_size), m_name(name), m_group(group)
{
    m_color = timer_colors[timer_color_index++ % (sizeof(timer_colors) / sizeof(timer_colors[0]))];
    m_new_weight = average_weight;
    m_old_weight = 1.0 - m_new_weight;
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

TimerFrontendManager::TimerFrontendManager(QObject* parent)
    :QObject(parent)
{
}

TimerFrontendManager::~TimerFrontendManager()
{
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    qDebug("~TimerFrontendManager()");
#endif
}

int TimerFrontendManager::current_frame = 0;
void TimerFrontendManager::receive_measurements(QList<nucleus::timing::TimerReport> values)
{
    for (const auto& report : values) {
        const char* name = report.timer->get_name().c_str();
        if (!m_timer_map.contains(name)) {
            auto tfo = new TimerFrontendObject(this, name, report.timer->get_group().c_str(), report.timer->get_queue_size(), report.timer->get_average_weight(), report.value);
            m_timer.append(tfo);
            m_timer_map.insert(name, tfo);
        }
        m_timer_map[name]->add_measurement(report.value, current_frame++);
    }
    emit updateTimingList(m_timer);
}

TimerFrontendManager::TimerFrontendManager(const TimerFrontendManager &src)
    :m_timer(src.m_timer)
{
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    qDebug() << "TimerFrontendManager()";
#endif
}

TimerFrontendManager& TimerFrontendManager::operator =(const TimerFrontendManager& other) {
    // Guard self assignment
    if (this == &other)
        return *this;

    this->m_timer = other.m_timer;
    return *this;
}

bool TimerFrontendManager::operator != (const TimerFrontendManager& other) {
    return this->m_timer.size() != other.m_timer.size();
}
