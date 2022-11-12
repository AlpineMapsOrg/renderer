#include "CrapyInteraction.h"

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


}
