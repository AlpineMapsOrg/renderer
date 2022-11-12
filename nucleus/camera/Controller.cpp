#include "Controller.h"

#include <QTimer>

#include "nucleus/camera/Definition.h"


namespace camera {
Controller::Controller(const Definition& camera)
    : m_definition(camera)
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
