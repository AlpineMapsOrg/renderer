#include "InteractionStyle.h"

using nucleus::camera::Definition;
using nucleus::camera::InteractionStyle;

std::optional<Definition> InteractionStyle::mouse_press_event(QMouseEvent*, Definition, float)
{
    return {};
}

std::optional<Definition> InteractionStyle::mouse_move_event(QMouseEvent*, Definition)
{
    return {};
}

std::optional<Definition> InteractionStyle::wheel_event(QWheelEvent* e, Definition camera, float distance)
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
