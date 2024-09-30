/*****************************************************************************
 * AlpineMaps.org
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
#include "Controller.h"

#include "AbstractDepthTester.h"
#include "CadInteraction.h"
#include "Definition.h"
#include "FirstPersonInteraction.h"
#include "LinearCameraAnimation.h"
#include "OrbitInteraction.h"
#include "RecordedAnimation.h"
#include "RotateNorthAnimation.h"
#include <QDebug>
#include <glm/gtx/string_cast.hpp>
#include <nucleus/DataQuerier.h>
#include <nucleus/srs.h>

using namespace nucleus::camera;

Controller::Controller(const Definition& camera, AbstractDepthTester* depth_tester, DataQuerier* data_querier)
    : m_definition(camera)
    , m_depth_tester(depth_tester)
    , m_data_querier(data_querier)
    , m_interaction_style(std::make_unique<OrbitInteraction>())
{
    connect(this, &Controller::definition_changed, &m_recorder, &recording::Device::record);
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

void Controller::fly_to_latitude_longitude(double latitude, double longitude)
{
    const auto xy_world_space = srs::lat_long_to_world({latitude, longitude});
    const auto look_at_point = glm::dvec3(xy_world_space,
                                          m_data_querier->get_altitude({latitude, longitude}));
    const auto camera_position = look_at_point + glm::normalize(glm::dvec3{0, -1, 1}) * 5000.;

    auto end_camera = m_definition;
    end_camera.look_at(camera_position, look_at_point);

    m_animation_style = std::make_unique<LinearCameraAnimation>(m_definition, end_camera);
    update();
}

void Controller::rotate_north()
{
    m_animation_style = std::make_unique<RotateNorthAnimation>(m_definition, m_depth_tester);
    update();
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
    report_global_cursor_position(QPointF(e.point.position.x, e.point.position.y));

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
    if (m_animation_style) {
        m_animation_style.reset();
        m_interaction_style->reset_interaction(m_definition, m_depth_tester);
    }
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
        m_interaction_style = std::make_unique<OrbitInteraction>();
    }
    if (e.key() == Qt::Key_2) {
        m_interaction_style = std::make_unique<FirstPersonInteraction>();
    }
    if (e.key() == Qt::Key_3) {
        m_interaction_style = std::make_unique<CadInteraction>();
    }
#if defined(ALP_ENABLE_DEV_TOOLS)
    if (e.key() == Qt::Key_8) {
        m_recorder.reset();
        m_recorder.start();
    }
    if (e.key() == Qt::Key_9) {
        m_recorder.stop();
    }
    if (e.key() == Qt::Key_0) {
        m_recorder.stop();
        m_animation_style = std::make_unique<RecordedAnimation>(m_recorder.recording());
        update();
    }
#endif

    const auto new_definition = m_interaction_style->key_press_event(e,
                                                                     m_definition,
                                                                     m_depth_tester);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
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
    if (m_animation_style) {
        m_animation_style.reset();
        m_interaction_style->reset_interaction(m_definition, m_depth_tester);
    }

    const auto new_definition = m_interaction_style->touch_event(e, m_definition, m_depth_tester);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::update_camera_request()
{
    if (m_animation_style) {
        const auto new_camera_definition = m_animation_style->update(m_definition, m_depth_tester);
        if (!new_camera_definition) {
            m_animation_style.reset();
            m_interaction_style->reset_interaction(m_definition, m_depth_tester);
            return;
        }
        m_definition = new_camera_definition.value();
        update();
    } else {
        const auto new_definition = m_interaction_style->update(m_definition, m_depth_tester);
        if (!new_definition)
            return;
        m_definition = new_definition.value();
        update();
    }
}

std::optional<glm::vec2> Controller::operation_centre()
{
    if (m_animation_style) {
        return m_animation_style->operation_centre();
    }
    return m_interaction_style->operation_centre();
}

std::optional<float> Controller::operation_centre_distance()
{
    if (m_animation_style) {
        return m_animation_style->operation_centre_distance(m_definition);
    }
    return m_interaction_style->operation_centre_distance(m_definition);
}

void Controller::report_global_cursor_position(const QPointF& screen_pos) {
    auto pos = m_depth_tester->position(m_definition.to_ndc({ screen_pos.x(), screen_pos.y() }));
    auto coord = srs::world_to_lat_long_alt(pos);
    emit global_cursor_position_changed(coord);
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

