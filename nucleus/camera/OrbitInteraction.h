/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
 * Copyright (C) 2024 Gerald Kimmersdorfer
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
class OrbitInteraction : public InteractionStyle
{
    glm::dvec3 m_operation_centre = {};
    glm::vec2 m_operation_centre_screen = {};
    bool m_move_vertical = false;
    bool m_key_ctrl = false;
    bool m_key_alt = false;
    bool m_key_shift = false;
public:
    std::optional<Definition> mouse_press_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> mouse_move_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> touch_event(const event_parameter::Touch& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> wheel_event(const event_parameter::Wheel& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> key_press_event(const QKeyCombination& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> key_release_event(const QKeyCombination& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<glm::vec2> operation_centre() override;

private:
    void start(const glm::vec2& position, const Definition& camera, AbstractDepthTester* depth_tester);
    void pan(const glm::vec2& position, const glm::vec2& last_position, Definition* camera, AbstractDepthTester* depth_tester);
    void orbit(const glm::vec2& position, const glm::vec2& last_position, Definition* camera, AbstractDepthTester* depth_tester);
    void zoom(float amount, Definition* camera, AbstractDepthTester* depth_tester);
};
}
