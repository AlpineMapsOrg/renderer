#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <QObject>

#include "sherpa/tile.h"

class Tile;

namespace camera {

class NearPlaneAdjuster : public QObject
{
    Q_OBJECT
public:
    explicit NearPlaneAdjuster(QObject *parent = nullptr);

signals:
    void nearPlaneChanged(float new_distance) const;

public slots:
    void changeCameraPosition(const glm::dvec3& new_position);
    void addTile(const std::shared_ptr<Tile>& tile);
    void removeTile(const tile::Id& tile_id);

private:
    void updateNearPlane() const;

    struct Sphere {
        tile::Id id;
        glm::dvec3 centre;
        double radius;
    };
    glm::dvec3 m_camera_position;
    std::vector<Sphere> m_object_spheres;
};

}
