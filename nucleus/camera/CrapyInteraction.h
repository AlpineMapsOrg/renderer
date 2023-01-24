#pragma once

#include "InteractionStyle.h"

namespace nucleus::camera {
class CrapyInteraction : public InteractionStyle
{
    glm::ivec2 m_previous_mouse_pos = { -1, -1 };
    glm::ivec2 m_previous_first_touch = { -1, -1 };
    glm::ivec2 m_previous_second_touch = { -1, -1 };
    bool m_was_double_touch = false;

public:
    std::optional<Definition> mouse_move_event(const event_parameter::Mouse& e, Definition camera) override;
    std::optional<Definition> touch_event(const event_parameter::Touch& e, Definition camera) override;
    std::optional<Definition> wheel_event(const event_parameter::Wheel& e, Definition camera) override;
};
}
