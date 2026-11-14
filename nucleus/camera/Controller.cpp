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
#include "RoutePreviewAnimation.h"
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
    const auto look_at_point = glm::dvec3(xy_world_space, m_data_querier->get_altitude({ latitude, longitude }).value_or(2000));
    const auto camera_position = look_at_point + glm::normalize(glm::dvec3{0, -1, 1}) * 5000.;

    auto end_camera = m_definition;
    end_camera.look_at(camera_position, look_at_point);

    m_animation_style = std::make_unique<LinearCameraAnimation>(m_definition, end_camera);
    update();
}

void Controller::move_to_latitude_longitude(double latitude, double longitude)
{
    const glm::dvec2 target_xy = srs::lat_long_to_world({ latitude, longitude });
    double target_altitude = 2000.0;
    if (m_data_querier) {
        target_altitude = m_data_querier->get_altitude({ latitude, longitude }).value_or(target_altitude);
    }

    // what the camera currently looks at: the terrain pixel at the screen
    // centre. without a depth tester, fall back to the camera position itself
    glm::dvec3 centre = m_definition.position();
    if (m_depth_tester) {
        const auto viewport = m_definition.viewport_size();
        centre = m_depth_tester->position(m_definition.to_ndc({ viewport.x / 2.0, viewport.y / 2.0 }));
    }

    // the shift moves the looked-at point onto the requested position; the
    // vertical component keeps the height above ground on sloped terrain
    const glm::dvec3 shift = glm::dvec3(target_xy, target_altitude) - centre;
    const glm::dvec3 new_position = m_definition.position() + shift;

    // keep the orientation: look along the unchanged view direction from the
    // shifted position (look_at derives the rotation from the direction only)
    const glm::dvec3 forward = m_definition.ray_direction(glm::dvec2(0.0));
    auto end_camera = m_definition;
    end_camera.look_at(new_position, new_position + forward);

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
    // Note: This function queries for the position a second time.
    report_global_cursor_position({ e.point.position.x, e.point.position.y });

    if (m_route_preview) {
        if (e.buttons & Qt::LeftButton) {
            m_route_preview->set_dragging(true);
        }
        return; // the preview owns camera control while it is active
    }

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
    if (m_route_preview) {
        if (e.buttons & Qt::LeftButton) {
            const glm::vec2 delta = e.point.position - e.point.last_position;
            m_route_preview->orbit(delta.x * 0.3, delta.y * 0.2);
        } else {
            m_route_preview->set_dragging(false);
        }
        return;
    }

    if (m_animation_style) {
        if (e.button == Qt::NoButton)
            return;
        m_animation_style.reset();
        m_interaction_style->reset_interaction(m_definition, m_depth_tester);
    }
    const auto new_definition = m_interaction_style->mouse_move_event(e, m_definition, m_depth_tester);
    if (!new_definition)
        return;

    m_definition = new_definition.value();

    update();
}

void Controller::mouse_release(const event_parameter::Mouse&)
{
    if (m_route_preview) {
        m_route_preview->set_dragging(false);
        return;
    }
}

void Controller::wheel_turn(const event_parameter::Wheel& e)
{
    if (m_route_preview) {
        m_route_preview->change_height(-double(e.angle_delta.y()) * 0.05);
        return;
    }

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
    if (m_route_preview) {
        switch (e.key()) {
        case Qt::Key_Left:
            m_route_preview->seek(std::max(0.0, m_route_preview->progress_fraction() - 0.05));
            break;
        case Qt::Key_Right:
            m_route_preview->seek(std::min(1.0, m_route_preview->progress_fraction() + 0.05));
            break;
        case Qt::Key_Up:
            m_route_preview->set_speed(m_route_preview->speed() * 1.25);
            break;
        case Qt::Key_Down:
            m_route_preview->set_speed(m_route_preview->speed() / 1.25);
            break;
        case Qt::Key_Escape:
            stop_route_preview();
            break;
        default:
            return;
        }
        emit_preview_state_if_changed();
        update();
        return;
    }

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
    if (m_route_preview) {
        if (e.points.size() == 1) {
            const auto& point = e.points[0];
            if (point.state == event_parameter::TouchPointPressed) {
                m_route_preview->set_dragging(true);
            } else if (point.state == event_parameter::TouchPointMoved && m_route_preview->paused()) {
                const glm::vec2 delta = point.position - point.last_position;
                m_route_preview->orbit(delta.x * 0.3, delta.y * 0.2);
            } else if (point.state == event_parameter::TouchPointReleased) {
                m_route_preview->set_dragging(false);
            }
        } else if (e.points.size() == 2) {
            // two finger pan: adjust the height above the terrain
            for (const auto& point : e.points) {
                if (point.state == event_parameter::TouchPointMoved) {
                    m_route_preview->change_height(-double(point.position.y - point.last_position.y) * 0.2);
                }
            }
        }
        return;
    }

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

void Controller::advance_camera()
{
    if (m_route_preview) {
        const auto new_camera_definition = m_route_preview->update(m_definition, m_depth_tester);
        if (new_camera_definition) {
            m_definition = new_camera_definition.value();
        }
        update();
        emit_preview_state_if_changed();
        return;
    }

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

void Controller::query_world_position(const QPointF& screen_pos)
{
    const glm::dvec3 pos = m_depth_tester->position(m_definition.to_ndc({ screen_pos.x(), screen_pos.y() }));
    emit world_position_queried(srs::world_to_lat_long_alt(pos));
}

void Controller::clear_keyboard_state()
{
    m_interaction_style->clear_keyboard_state();
}

void Controller::emit_preview_state_if_changed()
{
    if (!m_route_preview) {
        if (m_last_emitted_preview_active) {
            m_last_emitted_preview_active = false;
            emit route_preview_changed(0.0, false, false, 0.0);
        }
        return;
    }
    const double fraction = m_route_preview->progress_fraction();
    const bool paused = m_route_preview->paused();
    const double speed = m_route_preview->speed();
    // throttle: only emit on meaningful changes (the state changes every frame)
    if (m_last_emitted_preview_active && !paused && std::abs(fraction - m_last_emitted_preview_fraction) < 0.005
        && paused == m_last_emitted_preview_paused && speed == m_last_emitted_preview_speed) {
        return;
    }
    m_last_emitted_preview_fraction = fraction;
    m_last_emitted_preview_paused = paused;
    m_last_emitted_preview_speed = speed;
    m_last_emitted_preview_active = true;
    emit route_preview_changed(fraction, paused, true, speed);
}

void Controller::start_route_preview(const std::vector<glm::dvec3>& world_points)
{
    if (world_points.size() < 2) {
        return;
    }
    m_definition_before_preview = m_definition;
    m_route_preview = std::make_unique<RoutePreviewAnimation>(world_points);
    m_animation_style.reset();
    m_last_emitted_preview_fraction = -1.0;
    m_last_emitted_preview_speed = -1.0;
    update();
    emit_preview_state_if_changed();
}

void Controller::stop_route_preview()
{
    if (!m_route_preview) {
        return;
    }
    m_route_preview.reset();
    // return the camera to where it was before the fly-through started
    m_definition = m_definition_before_preview;
    m_interaction_style->reset_interaction(m_definition, m_depth_tester);
    emit_preview_state_if_changed();
    update();
}

void Controller::set_route_preview_paused(bool paused)
{
    if (m_route_preview) {
        m_route_preview->pause(paused);
        emit_preview_state_if_changed();
        update();
    }
}

void Controller::seek_route_preview(double fraction)
{
    if (m_route_preview) {
        m_route_preview->seek(fraction);
        emit_preview_state_if_changed();
        update();
    }
}

void Controller::set_route_preview_speed(double metres_per_second)
{
    if (m_route_preview) {
        m_route_preview->set_speed(metres_per_second);
        update();
    }
}

void Controller::set_pixel_error_threshold(float threshold)
{
    if (m_definition.pixel_error_threshold() == threshold)
        return;
    m_definition.set_pixel_error_threshold(threshold);
    update();
}

const Definition& Controller::definition() const
{
    return m_definition;
}

void Controller::set_model_matrix(const Definition& new_definition)
{
    if (m_definition.model_matrix() == new_definition.model_matrix())
        return;

    m_definition.set_model_matrix(new_definition.model_matrix());
    update();
}

