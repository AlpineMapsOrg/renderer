#include "NearPlaneAdjuster.h"

#include <limits>

#include "nucleus/Tile.h"

camera::NearPlaneAdjuster::NearPlaneAdjuster(QObject *parent)
    : QObject{parent}
{

}

void camera::NearPlaneAdjuster::changeCameraPosition(const glm::dvec3& new_position)
{
    m_camera_position = new_position;
    updateNearPlane();
}

void camera::NearPlaneAdjuster::addTile(const std::shared_ptr<Tile>& tile)
{
    m_object_spheres.emplace_back(tile->id, tile->bounds.centre(), glm::length(tile->bounds.size()) * 0.5);
    updateNearPlane();
}

void camera::NearPlaneAdjuster::removeTile(const tile::Id& tile_id)
{
    std::erase_if(m_object_spheres, [&tile_id](const Sphere& s) { return s.id == tile_id; });
    updateNearPlane();
}

void camera::NearPlaneAdjuster::updateNearPlane() const
{
    double min_dist = std::numeric_limits<double>::max();
    for (const auto& sphere : m_object_spheres) {
        const auto dist_to_cam = glm::length((sphere.centre - m_camera_position)) - sphere.radius;
        if (dist_to_cam < min_dist)
            min_dist = dist_to_cam;
    }
    min_dist = std::max(0.1, min_dist);
    if (!m_object_spheres.empty()) {
        emit nearPlaneChanged(min_dist);
    }
}
