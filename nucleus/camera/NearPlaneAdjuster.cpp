#include "NearPlaneAdjuster.h"

#include <limits>
#include <ranges>
#include <functional>

#include "Definition.h"
#include "nucleus/Tile.h"

using nucleus::camera::NearPlaneAdjuster;

NearPlaneAdjuster::NearPlaneAdjuster(QObject* parent)
    : QObject { parent }
{

}

void NearPlaneAdjuster::update_camera(const Definition& new_definition)
{
    const auto new_position = new_definition.position();
    if (m_camera_position == new_position)
        return;
    m_camera_position = new_position;
    update_near_plane();
}

void NearPlaneAdjuster::add_tile(const std::shared_ptr<Tile>& tile)
{
    m_objects.emplace_back(tile->id, tile->bounds.max.z);
    update_near_plane();
}

void NearPlaneAdjuster::remove_tile(const tile::Id& tile_id)
{
    std::erase_if(m_objects, [&tile_id](const Object& o) { return o.id == tile_id; });
    update_near_plane();
}

void NearPlaneAdjuster::update_near_plane() const
{
    if (!m_objects.empty()) {
        double max_elevation = 0;
        for (const auto& obj : m_objects) {
            if (obj.elevation > max_elevation)
                max_elevation = obj.elevation;
        }
        double near_plane_distance = std::max(10.0, (m_camera_position.z - max_elevation) * 0.8);
        emit near_plane_changed(near_plane_distance);
    }
}
