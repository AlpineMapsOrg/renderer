/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Adam Celarek
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

#include "SchedulerDirector.h"
#include "Scheduler.h"

using namespace nucleus::tile;

SchedulerDirector::SchedulerDirector()
    : QObject {}
{
}

bool SchedulerDirector::check_in(QString name, std::shared_ptr<Scheduler> scheduler)
{
    if (m_schedulers.contains(name))
        return false;
    m_schedulers[name] = scheduler;
    scheduler->set_name(name);
    return true;
}
