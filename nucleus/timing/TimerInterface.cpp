
#include "TimerInterface.h"

namespace nucleus::timing {

TimerInterface::TimerInterface(const std::string& name, const std::string& group, int queue_size, float average_weight)
    :m_name(name), m_group(group), m_queue_size(queue_size), m_average_weight(average_weight)
{
}

void TimerInterface::start() {
    //assert(m_state == TimerStates::READY);
    _start();
    m_state = TimerStates::RUNNING;
}

void TimerInterface::stop() {
    //assert(m_state == TimerStates::RUNNING);
    _stop();
    m_state = TimerStates::STOPPED;
}

bool TimerInterface::fetch_result() {
    if (m_state == TimerStates::STOPPED) {
        float val = _fetch_result();
        this->m_last_measurement = val;
        m_state = TimerStates::READY;
        return true;
    }
    return false;
}

}

