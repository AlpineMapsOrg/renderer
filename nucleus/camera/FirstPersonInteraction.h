/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
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

#pragma once

#include "InteractionStyle.h"

namespace nucleus::camera {
class FirstPersonInteraction : public InteractionStyle
{
    glm::ivec2 m_previous_mouse_pos = { -1, -1 };
    glm::ivec2 m_previous_first_touch = { -1, -1 };
    glm::ivec2 m_previous_second_touch = { -1, -1 };
    bool m_was_double_touch = false;
    float m_speed_modifyer = 13;
    bool m_key_w = false;
    bool m_key_s = false;
    bool m_key_a = false;
    bool m_key_d = false;
    bool m_key_e = false;
    bool m_key_q = false;
public:
    std::optional<Definition> mouse_move_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> touch_event(const event_parameter::Touch& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> wheel_event(const event_parameter::Wheel& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> key_press_event(const QKeyCombination& e, Definition camera, AbstractDepthTester* ray_caster) override;
    std::optional<Definition> key_release_event(const QKeyCombination& e, Definition camera, AbstractDepthTester* ray_caster) override;
};
}
