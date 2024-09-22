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

#pragma once

#include "Definition.h"
#include <QObject>
#include <chrono>
#include <nucleus/utils/Stopwatch.h>

namespace nucleus::camera::recording {

struct Frame {
    long msec = 0;
    glm::dmat4 camera_to_world_matrix = {};
};
using Animation = std::vector<Frame>;

class Device : public QObject {
    Q_OBJECT

public:
    Device();
    std::vector<Frame> recording() const;
    void reset();

public slots:
    void record(const Definition& def);
    void start();
    void stop();

private:
    bool m_enabled = false;
    utils::Stopwatch m_stopwatch = {};
    Animation m_frames;
};

} // namespace nucleus::camera::recording
