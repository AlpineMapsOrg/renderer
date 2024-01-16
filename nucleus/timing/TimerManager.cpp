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


#include "TimerManager.h"

#ifdef QT_DEBUG
#include <QDebug>
#endif

namespace nucleus::timing {

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
    return new_values;
}

std::shared_ptr<TimerInterface> TimerManager::add_timer(std::shared_ptr<TimerInterface> tmr) {
    m_timer[tmr->get_name()] = tmr;
    m_timer_in_order.push_back(tmr);
    return tmr;
}

}
