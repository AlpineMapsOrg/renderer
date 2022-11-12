#pragma once

#include <optional>

#include <QMouseEvent>
#include <QKeyEvent>

#include "Definition.h"

namespace camera {

class InteractionStyle
{
public:
    virtual std::optional<Definition> mousePressEvent(QMouseEvent* e, Definition camera, float distance);
    virtual std::optional<Definition> mouseMoveEvent(QMouseEvent* e, Definition camera);
    virtual std::optional<Definition> keyPressEvent(QKeyEvent* e, Definition camera);
};

}
