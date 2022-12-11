#pragma once

#include <memory>

#include <QObject>

#include <glm/glm.hpp>

#include "Definition.h"
#include "InteractionStyle.h"

namespace camera {

class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(const Definition& camera);

    [[nodiscard]] const Definition& definition() const;
    void set_interaction_style(std::unique_ptr<InteractionStyle> new_style);

public slots:
    void setDefinition(const Definition& new_definition);
    void setNearPlane(float distance);
    void setViewport(const glm::uvec2& new_viewport);
    void move(const glm::dvec3& v);
    void orbit(const glm::dvec3& centre, const glm::dvec2& degrees);
    void update() const;

    void mouse_press(QMouseEvent*, float distance);
    void mouse_move(QMouseEvent*);
    void key_press(const QKeyCombination&);
    void touch(QTouchEvent*);

signals:
    void definitionChanged(const Definition& new_definition) const;

private:
    Definition m_definition;
    std::unique_ptr<InteractionStyle> m_interaction_style;
};

}
