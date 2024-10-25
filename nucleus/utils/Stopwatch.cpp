/*****************************************************************************
 * AlpineMaps.org
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

#include "Stopwatch.h"

using namespace nucleus::utils;

Stopwatch::Stopwatch()
{
    restart();
}

void Stopwatch::restart()
{
    m_start = std::chrono::steady_clock::now();
    m_lap_start = m_start;
}

std::chrono::milliseconds Stopwatch::lap()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lap_start);
    m_lap_start = now;
    return elapsed;
}

std::chrono::milliseconds Stopwatch::total()
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start);;
}
