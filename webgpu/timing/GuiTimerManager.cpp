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

#include "GuiTimerManager.h"

namespace webgpu::timing {

GuiTimerManager::GuiTimerManager() = default;

std::shared_ptr<TimerInterface> GuiTimerManager::add_timer(std::shared_ptr<TimerInterface> tmr)
{
    // Your implementation here if needed
    return tmr;
}

const GuiTimerWrapper* GuiTimerManager::get_timer_by_id(uint32_t timer_id) const
{
    for (const auto& group : m_groups) {
        for (const auto& tmr : group.timers) {
            if (tmr.timer->get_id() == timer_id) {
                return &tmr;
            }
        }
    }
    return nullptr;
}

} // namespace webgpu::timing
