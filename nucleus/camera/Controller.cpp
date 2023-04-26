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

#include "Controller.h"

#include "nucleus/camera/CadInteraction.h"
#include "nucleus/camera/CrapyInteraction.h"
#include "nucleus/camera/Definition.h"
#include "nucleus/camera/FirstPersonInteraction.h"
#include "nucleus/camera/OrbitInteraction.h"
#include "nucleus/camera/RotateNorthInteraction.h"
#include "nucleus/srs.h"

namespace nucleus::camera {
Controller::Controller(const Definition& camera, AbstractDepthTester* depth_tester)
    : m_definition(camera)
    , m_depth_tester(depth_tester)
    , m_interaction_style(std::make_unique<InteractionStyle>())
{
    set_interaction_style(std::make_unique<nucleus::camera::OrbitInteraction>());
}


void Controller::set_near_plane(float distance)
{
    if (m_definition.near_plane() == distance)
        return;
    m_definition.set_near_plane(distance);
    update();
}

void Controller::set_viewport(const glm::uvec2& new_viewport)
{
    if (m_definition.viewport_size() == new_viewport)
        return;
    if (new_viewport.x * new_viewport.y == 0)
        return;
    m_definition.set_viewport_size(new_viewport);
    update();
}

void Controller::set_latitude_longitude(double latitude, double longitude)
{
    const auto xy_world_space = srs::lat_long_to_world({ latitude, longitude });
    move({ xy_world_space.x - m_definition.position().x,
        xy_world_space.y - m_definition.position().y,
        0.0 });
}

void Controller::set_field_of_view(float fov_degrees)
{
    if (qFuzzyCompare(m_definition.field_of_view(), fov_degrees))
        return;
    m_definition.set_field_of_view(fov_degrees);
    update();
}

void Controller::move(const glm::dvec3& v)
{
    if (v == glm::dvec3 { 0, 0, 0 })
        return;
    m_definition.move(v);
    update();
}

void Controller::orbit(const glm::dvec3& centre, const glm::dvec2& degrees)
{
    if (degrees == glm::dvec2 { 0, 0 })
        return;
    m_definition.orbit(centre, degrees);
    update();
}

void Controller::update() const
{
    emit definition_changed(m_definition);
}

void Controller::mouse_press(const event_parameter::Mouse& e)
{
    if (m_animation_style) {
        m_animation_style.reset();
        m_interaction_style->reset_interaction(m_definition, m_depth_tester);
    }
    const auto new_definition = m_interaction_style->mouse_press_event(e, m_definition, m_depth_tester);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::mouse_move(const event_parameter::Mouse& e)
{
    const auto new_definition = m_interaction_style->mouse_move_event(e, m_definition, m_depth_tester);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::wheel_turn(const event_parameter::Wheel& e)
{
    if (m_animation_style) {
        m_animation_style.reset();
        m_interaction_style->reset_interaction(m_definition, m_depth_tester);
    }

    const auto new_definition = m_interaction_style->wheel_event(e, m_definition, m_depth_tester);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::key_press(const QKeyCombination& e)
{
    if (m_animation_style) {
        m_animation_style.reset();
        m_interaction_style->reset_interaction(m_definition, m_depth_tester);
    }

    if (e.key() == Qt::Key_1) {
        set_interaction_style(std::make_unique<nucleus::camera::OrbitInteraction>());
    }
    if (e.key() == Qt::Key_2) {
        set_interaction_style(std::make_unique<nucleus::camera::FirstPersonInteraction>());
    }
    if (e.key() == Qt::Key_3) {
        set_interaction_style(std::make_unique<nucleus::camera::CadInteraction>());
    }
    if (e.key() == Qt::Key_4) {
        set_interaction_style(std::make_unique<nucleus::camera::CrapyInteraction>());
    }
    if (e.key() == Qt::Key_C) {
        set_animation_style(std::make_unique<nucleus::camera::RotateNorthInteraction>());
    }

    if (m_animation_style) {
        update();
    } else {
        const auto new_definition = m_interaction_style->key_press_event(e, m_definition, m_depth_tester);
        if (!new_definition)
            return;
        m_definition = new_definition.value();
        update();
    }
}

void Controller::key_release(const QKeyCombination& e)
{
    const auto new_definition = m_interaction_style->key_release_event(e, m_definition, m_depth_tester);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::touch(const event_parameter::Touch& e)
{
    const auto new_definition = m_interaction_style->touch_event(e, m_definition, m_depth_tester);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::update_camera_request()
{
    if (m_animation_style) {
        const auto new_animation = m_animation_style->update(m_definition, m_depth_tester);
        if (!new_animation) {
            m_animation_style.reset();
            m_interaction_style->reset_interaction(m_definition, m_depth_tester);
            return;
        }
        m_definition = new_animation.value();
        update();
    } else {
        const auto new_definition = m_interaction_style->update(m_definition, m_depth_tester);
        if (!new_definition)
            return;
        m_definition = new_definition.value();
        update();
    }
}

void Controller::set_interaction_style(std::unique_ptr<InteractionStyle> new_style)
{
    if (new_style)
        m_interaction_style = std::move(new_style);
    else
        m_interaction_style = std::make_unique<InteractionStyle>();
}

void Controller::set_animation_style(std::unique_ptr<InteractionStyle> new_style)
{
    if (new_style)
        m_animation_style = std::move(new_style);
    else
        m_animation_style = std::make_unique<InteractionStyle>();
}

std::optional<glm::vec2> Controller::get_operation_centre(){
    if (m_animation_style) {
        return m_animation_style->get_operation_centre();
    }
    return m_interaction_style->get_operation_centre();
}

const Definition& Controller::definition() const
{
    return m_definition;
}

void Controller::set_definition(const Definition& new_definition)
{
    if (m_definition.world_view_projection_matrix() == new_definition.world_view_projection_matrix())
        return;

    m_definition = new_definition;
    update();
}

}
