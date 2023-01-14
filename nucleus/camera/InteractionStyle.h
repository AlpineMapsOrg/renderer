#pragma once

#include <optional>

#include <QKeyCombination>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QWheelEvent>

#include "Definition.h"

namespace nucleus::camera {

class InteractionStyle
{
public:
    virtual ~InteractionStyle() = default;
    virtual std::optional<Definition> mouse_press_event(QMouseEvent* e, Definition camera, float distance);
    virtual std::optional<Definition> mouse_move_event(QMouseEvent* e, Definition camera);
    virtual std::optional<Definition> wheel_event(QWheelEvent* e, Definition camera, float distance);
    virtual std::optional<Definition> key_press_event(const QKeyCombination& e, Definition camera);
    virtual std::optional<Definition> touch_event(QTouchEvent* e, Definition camera);
};

}
