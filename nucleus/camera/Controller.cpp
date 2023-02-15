#include "Controller.h"

#include "nucleus/camera/Definition.h"

namespace nucleus::camera {
Controller::Controller(const Definition& camera)
    : m_definition(camera), m_interaction_style(std::make_unique<InteractionStyle>())
{
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
    m_definition.set_viewport_size(new_viewport);
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
    const auto new_definition = m_interaction_style->mouse_press_event(e, m_definition);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::mouse_move(const event_parameter::Mouse& e)
{
    const auto new_definition = m_interaction_style->mouse_move_event(e, m_definition);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::wheel_turn(const event_parameter::Wheel& e)
{
    const auto new_definition = m_interaction_style->wheel_event(e, m_definition);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::key_press(const QKeyCombination& e)
{
    const auto new_definition = m_interaction_style->key_press_event(e, m_definition);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::touch(const event_parameter::Touch& e)
{
    const auto new_definition = m_interaction_style->touch_event(e, m_definition);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::set_interaction_style(std::unique_ptr<InteractionStyle> new_style)
{
    if (new_style)
        m_interaction_style = std::move(new_style);
    else
        m_interaction_style = std::make_unique<InteractionStyle>();
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
