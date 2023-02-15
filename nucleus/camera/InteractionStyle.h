#pragma once

#include <optional>

#include <QKeyCombination>

#include "Definition.h"
#include "nucleus/event_parameter.h"

namespace nucleus::camera {

class InteractionStyle
{
public:
    virtual ~InteractionStyle() = default;
    virtual std::optional<Definition> mouse_press_event(const event_parameter::Mouse& e, Definition camera);
    virtual std::optional<Definition> mouse_move_event(const event_parameter::Mouse& e, Definition camera);
    virtual std::optional<Definition> wheel_event(const event_parameter::Wheel& e, Definition camera);
    virtual std::optional<Definition> key_press_event(const QKeyCombination& e, Definition camera);
    virtual std::optional<Definition> touch_event(const event_parameter::Touch& e, Definition camera);
};

}
