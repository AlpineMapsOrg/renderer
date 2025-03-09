 /*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
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

#include "PositionStorage.h"

namespace nucleus::camera {

PositionStorage* PositionStorage::s_instance = nullptr;

const std::map<std::string, Definition>& PositionStorage::positions() const { return m_positions; }

PositionStorage* PositionStorage::instance()
{
    if (s_instance == nullptr) {
        s_instance = new PositionStorage;
    }
    return s_instance;
}
}
