#include "InteractionStyle.h"


std::optional<camera::Definition> camera::InteractionStyle::mouse_press_event(QMouseEvent*, Definition, float)
{
    return {};
}

std::optional<camera::Definition> camera::InteractionStyle::mouse_move_event(QMouseEvent*, Definition)
{
    return {};
}

std::optional<camera::Definition> camera::InteractionStyle::wheel_event(QWheelEvent* e, Definition camera, float distance)
{
    return {};
}

std::optional<camera::Definition> camera::InteractionStyle::key_press_event(const QKeyCombination&, Definition)
{
    return {};
}

std::optional<camera::Definition> camera::InteractionStyle::touch_event(QTouchEvent*, Definition)
{
    return {};
}
