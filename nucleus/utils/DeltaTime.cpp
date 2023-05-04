/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Jakob Lindner
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

#include "DeltaTime.h"

using namespace nucleus::utils;

DeltaTime::DeltaTime()
{
    m_last_frame = std::chrono::steady_clock::now();
}

void DeltaTime::reset()
{
    m_last_frame = std::chrono::steady_clock::now();
}

std::chrono::milliseconds DeltaTime::get()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_frame);
    m_last_frame = now;
    return elapsed;
}
