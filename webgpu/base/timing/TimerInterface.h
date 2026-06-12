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

#include <QObject>
#include <string>
#include <vector>

namespace webgpu::timing {

std::string format_time(float time, int precision = 2);

class TimerInterface : public QObject {
    Q_OBJECT

public:
    TimerInterface(size_t capacity);
    virtual ~TimerInterface();

    uint32_t get_id();
    float get_last_measurement();
    size_t get_capacity();

    float get_average();
    float get_standard_deviation();
    size_t get_sample_count() const;

    float get_max() const;
    float get_min() const;

    void clear_results();
    const std::vector<float>& get_results() const;

    std::string to_string();

signals:
    void tick(float result);

protected:
    void add_result(float result);

private:
    std::vector<float> m_results;
    size_t m_capacity;
    float m_sum = 0.0f;
    float m_sum_of_squares = 0.0f;
    float m_max;
    float m_min;

    uint32_t m_id;

    static uint32_t s_next_id;
};

} // namespace webgpu::timing
