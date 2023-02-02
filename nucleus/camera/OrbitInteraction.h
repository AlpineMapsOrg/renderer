#pragma once

#include "InteractionStyle.h"

namespace nucleus::camera {
class OrbitInteraction : public InteractionStyle
{
    glm::ivec2 m_previous_mouse_pos = { -1, -1 };
    glm::ivec2 m_previous_first_touch = { -1, -1 };
    glm::ivec2 m_previous_second_touch = { -1, -1 };
public:
    std::optional<Definition> mouse_move_event(QMouseEvent* e, Definition camera) override;
    std::optional<Definition> touch_event(QTouchEvent* e, Definition camera) override;
    std::optional<Definition> wheel_event(QWheelEvent* e, Definition camera, float distance) override;
};
}
