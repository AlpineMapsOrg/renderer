#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <QObject>

#include "sherpa/tile.h"

struct Tile;

namespace camera {
class Definition;

class NearPlaneAdjuster : public QObject
{
    Q_OBJECT
public:
    explicit NearPlaneAdjuster(QObject *parent = nullptr);

signals:
    void nearPlaneChanged(float new_distance) const;

public slots:
    void updateCamera(const Definition& new_definition);
    void addTile(const std::shared_ptr<Tile>& tile);
    void removeTile(const tile::Id& tile_id);

private:
    void updateNearPlane() const;

    struct Object {
        Object() = default;
        Object(const tile::Id& id, const double& elevation) : id(id), elevation(elevation) {}
        tile::Id id;
        double elevation;
    };
    glm::dvec3 m_camera_position;
    std::vector<Object> m_objects;
};

}
