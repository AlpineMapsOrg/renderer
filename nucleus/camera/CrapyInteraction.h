#pragma once

#include "InteractionStyle.h"

namespace camera {
class CrapyInteraction : public InteractionStyle
{
    glm::ivec2 m_previous_mouse_pos = { -1, -1 };
    glm::ivec2 m_previous_first_touch = { -1, -1 };
    glm::ivec2 m_previous_second_touch = { -1, -1 };
public:
    std::optional<Definition> mouseMoveEvent(QMouseEvent* e, Definition camera) override;
    std::optional<Definition> touchEvent(QTouchEvent* e, Definition camera) override;
    std::optional<Definition> wheelEvent(QWheelEvent* e, Definition camera, float distance) override;
};
}
