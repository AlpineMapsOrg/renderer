#pragma once

#include <QObject>

#include <glm/glm.hpp>


namespace camera {
class Definition;

class Adapter : public QObject
{
    Q_OBJECT
public:
    explicit Adapter(Definition* camera = nullptr);

public slots:
    void setCamera(Definition* camera);
    void setNearPlane(float distance);
    void move(const glm::dvec3& v);
    void orbit(const glm::dvec3& centre, const glm::dvec2& degrees);
    void update() const;

signals:
    void positionChanged(const glm::dvec3& new_position) const;
    void worldViewChanged(const glm::dmat4& new_transformation) const;
    void projectionChanged(const glm::dmat4& new_transformation) const;
    void worldViewProjectionChanged(const glm::dmat4& new_transformation) const;

private:
    Definition* m_camera = nullptr;
};

}
