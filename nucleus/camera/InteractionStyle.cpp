#include "InteractionStyle.h"

using nucleus::camera::Definition;
using nucleus::camera::InteractionStyle;

std::optional<Definition> InteractionStyle::mouse_press_event(const nucleus::event_parameter::Mouse&, Definition)
{
    return {};
}

std::optional<Definition> InteractionStyle::mouse_move_event(const nucleus::event_parameter::Mouse&, Definition)
{
    return {};
}

std::optional<Definition> InteractionStyle::wheel_event(const nucleus::event_parameter::Wheel&, Definition)
{
    return {};
}

std::optional<Definition> InteractionStyle::key_press_event(const QKeyCombination&, Definition)
{
    return {};
}

std::optional<Definition> InteractionStyle::touch_event(const event_parameter::Touch&, Definition)
{
    return {};
}
