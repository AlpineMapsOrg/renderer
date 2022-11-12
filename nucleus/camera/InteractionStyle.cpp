#include "InteractionStyle.h"


std::optional<camera::Definition> camera::InteractionStyle::mousePressEvent(QMouseEvent*, Definition, float)
{
    return {};
}

std::optional<camera::Definition> camera::InteractionStyle::mouseMoveEvent(QMouseEvent*, Definition)
{
    return {};
}

std::optional<camera::Definition> camera::InteractionStyle::keyPressEvent(QKeyEvent*, Definition)
{
    return {};
}
