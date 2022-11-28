#include "NearPlaneAdjuster.h"

#include <limits>
#include <ranges>
#include <functional>

#include "Definition.h"
#include "nucleus/Tile.h"

camera::NearPlaneAdjuster::NearPlaneAdjuster(QObject *parent)
    : QObject{parent}
{

}

void camera::NearPlaneAdjuster::updateCamera(const Definition& new_definition)
{
    const auto new_position = new_definition.position();
    if (m_camera_position == new_position)
        return;
    m_camera_position = new_position;
    updateNearPlane();
}

void camera::NearPlaneAdjuster::addTile(const std::shared_ptr<Tile>& tile)
{
    m_objects.emplace_back(tile->id, tile->bounds.max.z);
    updateNearPlane();
}

void camera::NearPlaneAdjuster::removeTile(const tile::Id& tile_id)
{
    std::erase_if(m_objects, [&tile_id](const Object& o) { return o.id == tile_id; });
    updateNearPlane();
}

void camera::NearPlaneAdjuster::updateNearPlane() const
{
    if (!m_objects.empty()) {
        double max_elevation = 0;
        for (const auto& obj : m_objects) {
            if (obj.elevation > max_elevation)
                max_elevation = obj.elevation;
        }
        double near_plane_distance = std::max(10.0, (m_camera_position.z - max_elevation) * 0.8);
        emit nearPlaneChanged(near_plane_distance);
    }
}
