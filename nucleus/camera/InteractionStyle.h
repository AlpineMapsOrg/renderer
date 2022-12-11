#pragma once

#include <optional>

#include <QKeyCombination>
#include <QMouseEvent>
#include <QTouchEvent>

#include "Definition.h"

namespace camera {

class InteractionStyle
{
public:
    virtual ~InteractionStyle() = default;
    virtual std::optional<Definition> mousePressEvent(QMouseEvent* e, Definition camera, float distance);
    virtual std::optional<Definition> mouseMoveEvent(QMouseEvent* e, Definition camera);
    virtual std::optional<Definition> keyPressEvent(const QKeyCombination& e, Definition camera);
    virtual std::optional<Definition> touchEvent(QTouchEvent* e, Definition camera);
};

}
