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

#include "InteractionStyle.h"

using nucleus::camera::Definition;
using nucleus::camera::InteractionStyle;

void InteractionStyle::reset_interaction(Definition, AbstractDepthTester*) {}

std::optional<Definition> InteractionStyle::mouse_press_event(const nucleus::event_parameter::Mouse&, Definition, AbstractDepthTester*)
{
    return {};
}

std::optional<Definition> InteractionStyle::mouse_move_event(const nucleus::event_parameter::Mouse&, Definition, AbstractDepthTester*)
{
    return {};
}

std::optional<Definition> InteractionStyle::wheel_event(const nucleus::event_parameter::Wheel&, Definition, AbstractDepthTester*)
{
    return {};
}

std::optional<Definition> InteractionStyle::key_press_event(const QKeyCombination&, Definition, AbstractDepthTester*)
{
    return {};
}

std::optional<Definition> InteractionStyle::key_release_event(const QKeyCombination&, Definition, AbstractDepthTester*)
{
    return {};
}

std::optional<Definition> InteractionStyle::touch_event(const event_parameter::Touch&, Definition, AbstractDepthTester*)
{
    return {};
}

std::optional<Definition> InteractionStyle::update(Definition, AbstractDepthTester*)
{
    return {};
}

std::optional<glm::vec2> InteractionStyle::operation_centre()
{
    return {};
}

std::optional<float> InteractionStyle::operation_centre_distance(Definition)
{
    return {};
}
