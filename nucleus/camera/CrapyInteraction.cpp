#include "CrapyInteraction.h"

#include <QDebug>

namespace camera {

std::optional<Definition> CrapyInteraction::mouseMoveEvent(QMouseEvent* e, Definition camera)
{

    glm::ivec2 mouse_position { e->pos().x(), e->pos().y() };
    if (e->buttons() == Qt::LeftButton) {
        const auto delta = mouse_position - m_previous_mouse_pos;
        camera.pan(glm::vec2(delta) * 10.0f);
    }
    if (e->buttons() == Qt::MiddleButton) {
        const auto delta = mouse_position - m_previous_mouse_pos;
        camera.orbit(glm::vec2(delta) * 0.1f);
    }
    if (e->buttons() == Qt::RightButton) {
        const auto delta = mouse_position - m_previous_mouse_pos;
        camera.zoom(delta.y * 10.0);
    }
    m_previous_mouse_pos = mouse_position;

    if (e->buttons() == Qt::NoButton)
        return {};
    else
        return camera;
}

std::optional<Definition> CrapyInteraction::touchEvent(QTouchEvent* e, Definition camera)
{
    glm::ivec2 first_touch = { e->points()[0].position().x(), e->points()[0].position().y() };
    glm::ivec2 second_touch;
    if (e->points().size() >= 2)
        second_touch = { e->points()[1].position().x(), e->points()[1].position().y() };

    if (e->isEndEvent())
        return {};
    if (e->isBeginEvent()) {
        m_previous_first_touch = first_touch;
        m_previous_second_touch = second_touch;
        return {};
    }
    // touch move
    if (e->points().size() == 1) {
        const auto delta = first_touch - m_previous_first_touch;
        camera.pan(glm::vec2(delta) * 10.0f);
    }
    if (e->points().size() == 2) {
        const auto previous_centre = (m_previous_first_touch + m_previous_second_touch) / 2;
        const auto current_centre = (first_touch + second_touch) / 2;
        const auto pitch = -(current_centre - previous_centre).y;

        const auto previous_yaw_dir = glm::normalize(glm::vec2(m_previous_first_touch - m_previous_second_touch));
        const auto previous_yaw_angle = std::atan2(previous_yaw_dir.y, previous_yaw_dir.x);
        const auto current_yaw_dir = glm::normalize(glm::vec2(first_touch - second_touch));
        const auto current_yaw_angle = std::atan2(current_yaw_dir.y, current_yaw_dir.x);
//        qDebug() << "prev: " << previous_yaw_angle << ", curr: " << current_yaw_angle;

        const auto yaw = (current_yaw_angle - previous_yaw_angle) * 600;
        camera.orbit(glm::vec2(yaw, pitch) * 0.1f);

        const auto previous_dist = glm::length(glm::vec2(m_previous_first_touch - m_previous_second_touch));
        const auto current_dist = glm::length(glm::vec2(first_touch - second_touch));
        camera.zoom((previous_dist - current_dist) * 10);
    }
    m_previous_first_touch = first_touch;
    m_previous_second_touch = second_touch;
    return camera;
}


}
