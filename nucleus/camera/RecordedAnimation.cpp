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

#include "RecordedAnimation.h"

#include <QDebug>

nucleus::camera::RecordedAnimation::RecordedAnimation(const recording::Animation& animation)
    : m_animation(animation)
{
    m_stopwatch.restart();
}

std::optional<nucleus::camera::Definition> nucleus::camera::RecordedAnimation::update(Definition camera, AbstractDepthTester*)
{
    const auto current_time = m_stopwatch.total().count();

    // slower than need be, but shouldn't matter with recordings of reasonable length.
    const auto frame_right_iter = std::find_if(m_animation.begin(), m_animation.end(), [&](const auto& f) { return f.msec > current_time; });

    if (frame_right_iter == m_animation.begin())
        return camera; // we don't care about the start
    if (frame_right_iter == m_animation.end()) {
        m_stopwatch.restart();
        return camera;
    }
    const auto fl = *(frame_right_iter - 1);
    const auto fr = *(frame_right_iter);
    // there are no frames if the camera stops moving. mixing causes a slow drift to the next position. doesn't look good.
    // const auto mix = float(current_time - fl.msec) / float(fr.msec - fl.msec);
    const auto mix = 0.f;
    const auto new_matrix = fl.camera_to_world_matrix * double(1 - mix) + fr.camera_to_world_matrix * double(mix);

    camera.set_camera_space_to_world_matrix(new_matrix);
    return camera;
}
