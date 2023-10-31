#include "TimerManager.h"

#ifdef QT_DEBUG
#include <QDebug>
#endif

namespace nucleus::timing {

std::unique_ptr<TimerManager> TimerManager::instance = nullptr;
std::once_flag TimerManager::onceInitFlag;

TimerManager* TimerManager::getInstance() {
    std::call_once(onceInitFlag, []() {
        instance.reset(new TimerManager());
    });
    return instance.get();
}

TimerManager::TimerManager()
{
}

#ifdef QT_DEBUG
void TimerManager::warn_about_timer(const std::string& name) {
    if (!m_timer_already_warned_about.contains(name)) {
        qWarning() << "Requested Timer with name: " << name << " which has not been created.";
        m_timer_already_warned_about.insert(name);
    }
}
#endif

void TimerManager::start_timer(const std::string &name)
{
    auto it = m_timer.find(name);
    if (it != m_timer.end()) m_timer[name]->start();
#ifdef QT_DEBUG
    else warn_about_timer(name);
#endif
}

void TimerManager::stop_timer(const std::string &name)
{
    auto it = m_timer.find(name);
    if (it != m_timer.end()) m_timer[name]->stop();
#ifdef QT_DEBUG
    else warn_about_timer(name);
#endif
}

QList<TimerReport> TimerManager::fetch_results()
{
    QList<TimerReport> new_values;
    for (const auto& tmr : m_timer_in_order) {
        if (tmr->fetch_result()) {
            new_values.push_back({ tmr->get_last_measurement(), tmr });
        }
    }
    return std::move(new_values);
}

std::shared_ptr<TimerInterface> TimerManager::add_timer(TimerInterface* tmr) {
    std::shared_ptr<TimerInterface> tmr_shared(tmr);
    m_timer[tmr_shared->get_name()] = tmr_shared;
    m_timer_in_order.push_back(tmr_shared);
    return tmr_shared;
}

/*
std::shared_ptr<GeneralTimer> TimerManager::add_timer(const std::string &name, TimerTypes type, const std::string& group, int queue_size, float average_weight) {
    std::shared_ptr<GeneralTimer> tmr_base;
    switch(type) {

    case TimerTypes::CPU:
        tmr_base = static_pointer_cast<GeneralTimer>(std::make_shared<HostTimer>(name, group, queue_size, average_weight));
        break;
#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64) // only on native
    case TimerTypes::GPU:
        tmr_base = static_pointer_cast<GeneralTimer>(std::make_shared<GpuSyncQueryTimer>(name, group, queue_size, average_weight));
        break;

    case TimerTypes::GPUAsync:
        tmr_base = static_pointer_cast<GeneralTimer>(std::make_shared<GpuAsyncQueryTimer>(name, group, queue_size, average_weight));
        break;
#endif
    default:
        qDebug() << "Timertype " << (int)type << " not supported on current target";
        return nullptr;
    }

    m_timer[name] = tmr_base;
    m_timer_in_order.push_back(tmr_base);
    return tmr_base;
}
*/
}
