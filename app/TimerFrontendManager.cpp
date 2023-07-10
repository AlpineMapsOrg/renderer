#include "TimerFrontendManager.h"
#include "gl_engine/TimerManager.h"

#include <QMap>
#include <QDebug>

int TimerFrontendObject::timer_color_index = 0;

void TimerFrontendObject::add_measurement(float value) {
    m_quick_average = m_quick_average * m_old_weight + value * m_new_weight;
    m_measurements.append(value);
    if (m_measurements.size() > m_queue_size) m_measurements.removeFirst();
}

float TimerFrontendObject::get_last_measurement() {
    return m_measurements[m_measurements.length() - 1];
}

float TimerFrontendObject::get_average() {
    float sum = 0.0f;
    for (int i=0; i<m_measurements.count(); i++) {
        sum += m_measurements[i];
    }
    return sum / m_measurements.size();
}

TimerFrontendObject::TimerFrontendObject(const QString& name, const QString& group, const int queue_size, const float average_weight, const float first_value)
    :m_queue_size(queue_size), m_name(name), m_group(group)
{
    m_color = timer_colors[timer_color_index++ % (sizeof(timer_colors) / sizeof(timer_colors[0]))];
    m_new_weight = average_weight;
    m_old_weight = 1.0 - m_new_weight;
    m_quick_average = first_value;
}


TimerFrontendManager::TimerFrontendManager()
{
}

TimerFrontendManager::~TimerFrontendManager()
{
    for (int i = 0; i < m_timer.count(); i++) {
        delete m_timer[i];
    }
}

void TimerFrontendManager::receive_measurements(QList<gl_engine::qTimerReport> values)
{
    for (const auto& report : values) {
        const char* name = report.timer->get_name().c_str();
        if (!m_timer_map.contains(name)) {
            auto tfo = new TimerFrontendObject(name, report.timer->get_group().c_str(), report.timer->get_queue_size(), report.timer->get_average_weight(), report.value);
            m_timer.append(tfo);
            m_timer_map.insert(name, tfo);
        }
        m_timer_map[name]->add_measurement(report.value);
    }
    emit updateTimingList(m_timer);
}

TimerFrontendManager::TimerFrontendManager(const TimerFrontendManager &src)
    :m_timer(src.m_timer)
{

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
