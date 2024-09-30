/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
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

#include "recording.h"

nucleus::camera::recording::Device::Device() { }

std::vector<nucleus::camera::recording::Frame> nucleus::camera::recording::Device::recording() const { return m_frames; }

void nucleus::camera::recording::Device::reset()
{
    m_enabled = false;
    m_frames.resize(0);
}

void nucleus::camera::recording::Device::record(const Definition& def)
{
    if (!m_enabled)
        return;
    m_frames.push_back({ uint(m_stopwatch.total().count()), def.camera_space_to_world_matrix() });
}

void nucleus::camera::recording::Device::start()
{
    m_enabled = true;
    m_stopwatch.restart();
}

void nucleus::camera::recording::Device::stop() { m_enabled = false; }
