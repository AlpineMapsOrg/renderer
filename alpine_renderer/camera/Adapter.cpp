#include "Adapter.h"

#include <QTimer>

#include "alpine_renderer/camera/Definition.h"

camera::Adapter::Adapter(camera::Definition* camera)
    : m_camera(camera)
{
}

void camera::Adapter::setCamera(camera::Definition* camera)
{
    m_camera = camera;
    if (m_camera != nullptr) {
        emit positionChanged(m_camera->position());
        emit worldViewChanged(m_camera->cameraMatrix());
        emit projectionChanged(m_camera->projectionMatrix());
        emit worldViewProjectionChanged(m_camera->worldViewProjectionMatrix());
    }
}

void camera::Adapter::setNearPlane(float distance)
{
    if (m_camera != nullptr) {
        m_camera->setNearPlane(distance);
        emit projectionChanged(m_camera->projectionMatrix());
        emit worldViewProjectionChanged(m_camera->worldViewProjectionMatrix());
    }
}

void camera::Adapter::move(const glm::dvec3& v)
{
    if (m_camera != nullptr) {
        m_camera->move(v);
        emit positionChanged(m_camera->position());
        emit worldViewChanged(m_camera->cameraMatrix());
        emit worldViewProjectionChanged(m_camera->worldViewProjectionMatrix());
    }
}

void camera::Adapter::orbit(const glm::dvec3& centre, const glm::dvec2& degrees)
{

    if (m_camera != nullptr) {
        m_camera->orbit(centre, degrees);
        emit positionChanged(m_camera->position());
        emit worldViewChanged(m_camera->cameraMatrix());
        emit worldViewProjectionChanged(m_camera->worldViewProjectionMatrix());
    }
}

void camera::Adapter::update() const
{
    emit positionChanged(m_camera->position());
    emit worldViewChanged(m_camera->cameraMatrix());
    emit projectionChanged(m_camera->projectionMatrix());
    emit worldViewProjectionChanged(m_camera->worldViewProjectionMatrix());
}
