#pragma once

#include <memory>

#include <QObject>

#include <glm/glm.hpp>

#include "Definition.h"
#include "InteractionStyle.h"

namespace nucleus::camera {

class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(const Definition& camera);

    [[nodiscard]] const Definition& definition() const;
    void set_interaction_style(std::unique_ptr<InteractionStyle> new_style);

public slots:
    void set_definition(const Definition& new_definition);
    void set_near_plane(float distance);
    void set_viewport(const glm::uvec2& new_viewport);
    void move(const glm::dvec3& v);
    void orbit(const glm::dvec3& centre, const glm::dvec2& degrees);
    void update() const;

    void mouse_press(QMouseEvent*, float);
    void mouse_move(QMouseEvent*);
    void wheel_turn(QWheelEvent*, float);
    void key_press(const QKeyCombination&);
    void touch(QTouchEvent*);

signals:
    void definition_changed(const Definition& new_definition) const;

private:
    Definition m_definition;
    std::unique_ptr<InteractionStyle> m_interaction_style;
};

}
