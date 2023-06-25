#include "TimerFrontendManager.h"

TimerFrontendManager::TimerFrontendManager()
{
    m_timer_names.append("test23");
}

TimerFrontendManager::~TimerFrontendManager()
{
    // Nothing to do here, but necessary for QML
}

TimerFrontendManager::TimerFrontendManager(const TimerFrontendManager &src)
    :m_timer_names(src.m_timer_names)
{

}

TimerFrontendManager& TimerFrontendManager::operator =(const TimerFrontendManager& other) {
    // Guard self assignment
    if (this == &other)
        return *this;

    this->m_timer_names = other.m_timer_names;
    return *this;
}

bool TimerFrontendManager::operator != (const TimerFrontendManager& other) {
    return this->m_timer_names != other.m_timer_names;
}
