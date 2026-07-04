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

#pragma once

#include "TimerInterface.h"
#include <QDebug>
#include <QObject>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace webgpu::timing {

struct GuiTimerWrapper {
    std::shared_ptr<TimerInterface> timer;
    std::string name;
    std::string group;
    glm::vec4 color;
};

struct GuiTimerGroup {
    std::string name;
    std::vector<GuiTimerWrapper> timers;
};

class GuiTimerManager : public QObject {
    Q_OBJECT

public:
    // Adds the given timer
    std::shared_ptr<TimerInterface> add_timer(std::shared_ptr<TimerInterface> tmr);

    template <typename T, typename = std::enable_if_t<std::is_base_of<TimerInterface, T>::value>>
    void add_timer(std::shared_ptr<T> tmr, const std::string& name, const std::string& group = "", const glm::vec4& color = glm::vec4(-1.0f))
    {
        std::shared_ptr<TimerInterface> timer = std::dynamic_pointer_cast<TimerInterface>(tmr);
        if (timer) {
            auto tmrColor = color;
            if (tmrColor.x < 0.0f) {
                tmrColor = glm::vec4(timer_colors[timer->get_id() % 12], 1.0f);
            }
            auto it = std::find_if(m_groups.begin(), m_groups.end(), [&](const GuiTimerGroup& g) { return g.name == group; });
            if (it != m_groups.end()) {
                it->timers.push_back({ timer, name, group, tmrColor });
            } else {
                GuiTimerGroup newGroup { group, { { timer, name, group, tmrColor } } };
                m_groups.push_back(newGroup);
            }
        } else {
            qCritical() << "Timer can't be added as it's not initialized correctly";
        }
    }
    [[nodiscard]] const GuiTimerWrapper* get_timer_by_id(uint32_t timer_id) const;

    [[nodiscard]] const std::vector<GuiTimerGroup>& get_groups() const { return m_groups; }

    GuiTimerManager();

private:
    // Contains the timer groups
    std::vector<GuiTimerGroup> m_groups;

    static constexpr glm::vec3 timer_colors[] = {
        glm::vec3(1.0f, 0.0f, 0.0f), // red
        glm::vec3(0.0f, 1.0f, 1.0f), // cyan
        glm::vec3(0.49f, 0.0f, 1.0f), // violet
        glm::vec3(0.49f, 1.0f, 0.0f), // spring green
        glm::vec3(1.0f, 0.0f, 1.0f), // magenta
        glm::vec3(0.0f, 0.49f, 1.0f), // ocean
        glm::vec3(0.0f, 1.0f, 0.0f), // green
        glm::vec3(1.0f, 0.49f, 0.0f), // orange
        glm::vec3(0.0f, 0.0f, 1.0f), // blue
        glm::vec3(0.0f, 1.0f, 0.49f), // turquoise
        glm::vec3(1.0f, 1.0f, 0.0f), // yellow
        glm::vec3(1.0f, 0.0f, 0.49f) // raspberry
    };
};

} // namespace webgpu::timing
