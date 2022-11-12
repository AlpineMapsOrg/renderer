#pragma once

#include "InteractionStyle.h"

namespace camera {
class CrapyInteraction : public InteractionStyle
{
    glm::ivec2 m_previous_mouse_pos = { -1, -1 };
protected:
    std::optional<Definition> mouseMoveEvent(QMouseEvent* e, Definition camera) override;
};
}
