#pragma once

#include <QObject>

#include <glm/glm.hpp>

#include "Definition.h"

namespace camera {

class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(const Definition& camera);

    [[nodiscard]] const Definition& definition() const;

public slots:
    void setDefinition(const Definition& new_definition);
    void setNearPlane(float distance);
    void setViewport(const glm::uvec2& new_viewport);
    void move(const glm::dvec3& v);
    void orbit(const glm::dvec3& centre, const glm::dvec2& degrees);
    void update() const;

signals:
    void definitionChanged(const Definition& new_definition) const;

private:
    Definition m_definition;
};

}
