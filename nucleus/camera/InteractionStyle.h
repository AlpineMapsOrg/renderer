/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2022 Adam Celarek
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

#include <optional>

#include <QKeyCombination>

#include "Definition.h"
#include "nucleus/event_parameter.h"

namespace nucleus::camera {
class AbstractDepthTester;

class InteractionStyle
{
public:
    virtual ~InteractionStyle() = default;
    virtual void reset_interaction(Definition camera, AbstractDepthTester* depth_tester);
    virtual std::optional<Definition> mouse_press_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester);
    virtual std::optional<Definition> mouse_move_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester);
    virtual std::optional<Definition> wheel_event(const event_parameter::Wheel& e, Definition camera, AbstractDepthTester* depth_tester);
    virtual std::optional<Definition> key_press_event(const QKeyCombination& e, Definition camera, AbstractDepthTester* depth_tester);
    virtual std::optional<Definition> key_release_event(const QKeyCombination& e, Definition camera, AbstractDepthTester* depth_tester);
    virtual std::optional<Definition> touch_event(const event_parameter::Touch& e, Definition camera, AbstractDepthTester* depth_tester);
    virtual std::optional<Definition> update(Definition camera, AbstractDepthTester* depth_tester);
    virtual std::optional<glm::vec2> operation_centre();
    virtual std::optional<float> operation_centre_distance(Definition camera);
};

}
