#include "Controller.h"

#include "nucleus/camera/Definition.h"


namespace camera {
Controller::Controller(const Definition& camera)
    : m_definition(camera), m_interaction_style(std::make_unique<InteractionStyle>())
{
}


void Controller::setNearPlane(float distance)
{
    if (m_definition.nearPlane() == distance)
        return;
    m_definition.setNearPlane(distance);
    update();
}

void Controller::setViewport(const glm::uvec2& new_viewport)
{
    if (m_definition.viewportSize() == new_viewport)
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
    emit definitionChanged(m_definition);
}

void Controller::mouse_press(QMouseEvent* e, float distance)
{
    const auto new_definition = m_interaction_style->mousePressEvent(e, m_definition, distance);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::mouse_move(QMouseEvent* e)
{
    const auto new_definition = m_interaction_style->mouseMoveEvent(e, m_definition);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::wheel_turn(QWheelEvent* e, float distance)
{
    const auto new_definition = m_interaction_style->wheelEvent(e, m_definition, distance);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::key_press(const QKeyCombination& e)
{
    const auto new_definition = m_interaction_style->keyPressEvent(e, m_definition);
    if (!new_definition)
        return;
    m_definition = new_definition.value();
    update();
}

void Controller::touch(QTouchEvent* e)
{
    const auto new_definition = m_interaction_style->touchEvent(e, m_definition);
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

void Controller::setDefinition(const Definition& new_definition)
{
    m_definition = new_definition;
    update();
}

}
