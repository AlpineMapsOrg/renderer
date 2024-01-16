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

#include <string>
#include <QDebug>

namespace nucleus::timing {

enum class TimerStates { READY, RUNNING, STOPPED };

class TimerInterface {

public:

    TimerInterface(const std::string& name, const std::string& group, int queue_size, float average_weight);
    virtual ~TimerInterface()
    {
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
        qDebug() << "nucleus::timing::~TimerInterface(name=" << get_name() << ")";
#endif
    }

    // Starts time-measurement
    void start();

    // Stops time-measurement
    void stop();

    // Fetches the result of the measuring and adds it to the average
    bool fetch_result();

    const std::string& get_name()       {   return this->m_name;                    }
    const std::string& get_group()      {   return this->m_group;                   }
    float get_last_measurement()        {   return this->m_last_measurement;        }
    int get_queue_size()                {   return this->m_queue_size;              }
    float get_average_weight()          {   return this->m_average_weight;          }


protected:
    // a custom identifying name for this timer
    std::string m_name;
    std::string m_group;
    int m_queue_size;
    float m_average_weight;

    virtual void _start() = 0;
    virtual void _stop() = 0;
    virtual float _fetch_result() = 0;

private:

    TimerStates m_state = TimerStates::READY;
    float m_last_measurement = 0.0f;
};

}
