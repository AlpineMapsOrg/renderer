#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <QObject>

#include "sherpa/tile.h"

struct Tile;

namespace nucleus::camera {
class Definition;

class NearPlaneAdjuster : public QObject
{
    Q_OBJECT
public:
    explicit NearPlaneAdjuster(QObject *parent = nullptr);

signals:
    void near_plane_changed(float new_distance) const;

public slots:
    void update_camera(const Definition& new_definition);
    void add_tile(const std::shared_ptr<Tile>& tile);
    void remove_tile(const tile::Id& tile_id);

private:
    void update_near_plane() const;

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
