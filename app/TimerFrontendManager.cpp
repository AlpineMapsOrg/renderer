#include "TimerFrontendManager.h"
#include "gl_engine/TimerManager.h"

#include <QMap>
#include <QDebug>

int TimerFrontendObject::timer_color_index = 0;

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
            auto tfo = new TimerFrontendObject(name, report.timer->get_group().c_str(), report.timer->get_queue_size());
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
