#include "CpuTimer.h"

namespace nucleus::timing {

CpuTimer::CpuTimer(const std::string &name, const std::string& group, int queue_size, const float average_weight)
    :TimerInterface(name, group, queue_size, average_weight)
{
}

void CpuTimer::_start() {
    m_ticks[0] = std::chrono::high_resolution_clock::now();
}

void CpuTimer::_stop() {
    m_ticks[1] = std::chrono::high_resolution_clock::now();
}

float CpuTimer::_fetch_result() {
    std::chrono::duration<double> diff = m_ticks[1] - m_ticks[0];
    return ((float)(diff.count() * 1000.0));
}

}
