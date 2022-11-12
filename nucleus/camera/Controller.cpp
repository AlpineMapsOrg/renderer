#include "Controller.h"

#include <QTimer>

#include "nucleus/camera/Definition.h"

camera::Controller::Controller(camera::Definition* camera)
    : m_camera(camera)
{
}

void camera::Controller::setCamera(camera::Definition* camera)
{
    m_camera = camera;
    if (m_camera != nullptr) {
        emit positionChanged(m_camera->position());
        emit worldViewChanged(m_camera->cameraMatrix());
        emit projectionChanged(m_camera->projectionMatrix());
        emit worldViewProjectionChanged(m_camera->worldViewProjectionMatrix());
    }
}

void camera::Controller::setNearPlane(float distance)
{
    if (m_camera != nullptr) {
        m_camera->setNearPlane(distance);
        emit projectionChanged(m_camera->projectionMatrix());
        emit worldViewProjectionChanged(m_camera->worldViewProjectionMatrix());
    }
}

void camera::Controller::move(const glm::dvec3& v)
{
    if (m_camera != nullptr) {
        m_camera->move(v);
        emit positionChanged(m_camera->position());
        emit worldViewChanged(m_camera->cameraMatrix());
        emit worldViewProjectionChanged(m_camera->worldViewProjectionMatrix());
    }
}

void camera::Controller::orbit(const glm::dvec3& centre, const glm::dvec2& degrees)
{

    if (m_camera != nullptr) {
        m_camera->orbit(centre, degrees);
        emit positionChanged(m_camera->position());
        emit worldViewChanged(m_camera->cameraMatrix());
        emit worldViewProjectionChanged(m_camera->worldViewProjectionMatrix());
    }
}

void camera::Controller::update() const
{
    emit positionChanged(m_camera->position());
    emit worldViewChanged(m_camera->cameraMatrix());
    emit projectionChanged(m_camera->projectionMatrix());
    emit worldViewProjectionChanged(m_camera->worldViewProjectionMatrix());
}
