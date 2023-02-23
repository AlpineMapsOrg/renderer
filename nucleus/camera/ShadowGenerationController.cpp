#include "ShadowGenerationController.h"

#include "nucleus/srs.h"

#include <algorithm>
#include <QDebug>

using namespace camera;

constexpr float CONVERSION_FACTOR = 0.125f;

static bool containsCoordinate(const tile::Id& tile, const glm::vec2& coordinate) {
    const auto tile_bounds = srs::tile_bounds(tile);

    return tile_bounds.contains(coordinate);
}

ShadowGenerationController::ShadowGenerationController(const glm::vec2& reference_point, unsigned int zoom, tile::Scheme scheme) {

    tile::Id root = {0, {0, 0}, scheme};

    do {
        unsigned int last_zoom = root.zoom_level;

        for (const auto& child : root.children()) {
            if (containsCoordinate(child, reference_point)) {
                root = child;
                break;
            }
        }

        if (last_zoom == root.zoom_level) { //loop didn't break - error
            assert(0);
        }
    } while (root.zoom_level != zoom);

    m_center_tile = root;
}

camera::Definition ShadowGenerationController::tile_cam() const {
    auto tile_bounds = srs::tile_bounds(m_center_tile);

    float min, max, center;
    get_min_max_center(m_center_tile, min, max, center);

    const unsigned int z_offset = 10;
    auto cam_pos = glm::vec3(tile_bounds.centre(), max + z_offset);

    camera::Definition cam_def(cam_pos, cam_pos - glm::vec3(0, 0, z_offset));

    cam_def.setOrthographicParams(-tile_bounds.width() * 0.5, tile_bounds.width() * 0.5, -tile_bounds.height() * 0.5, tile_bounds.height() * 0.5, z_offset, 2 * z_offset + (max - min));

    return cam_def;
}

camera::Definition ShadowGenerationController::sun_cam(const glm::vec2& orbit, const glm::vec2& range, float z_offset) {
    auto tile_bounds = srs::tile_bounds(m_center_tile);

    float min, max, center;
    get_min_max_center(m_center_tile, min, max, center);

    glm::vec3 cam_pos = glm::vec3(tile_bounds.centre(), center + z_offset);
    glm::vec3 point_of_interest = glm::vec3(tile_bounds.centre(), center);
    camera::Definition cam(cam_pos, point_of_interest);

    cam.orbit(point_of_interest, orbit);

    cam.setOrthographicParams(-tile_bounds.width() * 0.5 * range.x, tile_bounds.width() * 0.5 * range.x,
                              -tile_bounds.width() * 0.5 * range.y, tile_bounds.width() * 0.5 * range.y,
                              0, z_offset + glm::max(tile_bounds.width(), tile_bounds.height()));
    return cam;
}

void ShadowGenerationController::get_min_max_center(const tile::Id& tile, float& min, float& max, float& center) const{
    const std::vector<uint16_t>& heights = m_tile_heights.at(m_center_tile).buffer();

    auto [uint_min, uint_max] = std::minmax_element(heights.begin(), heights.end());
    min = CONVERSION_FACTOR * (*uint_min);
    max = CONVERSION_FACTOR * (*uint_max);
    center = CONVERSION_FACTOR * heights.at(heights.size() / 2);
}

const Raster<uint16_t>& ShadowGenerationController::get_height_map(const tile::Id& tile) {
    return m_tile_heights.at(tile);
}

const tile::Id& ShadowGenerationController::center_tile() const {
    return m_center_tile;
}

const QImage& ShadowGenerationController::ortho_texture() const {
    return m_ortho_textures.at(m_center_tile);
}

void ShadowGenerationController::advance(glm::ivec2 steps) {
    m_center_tile.coords += steps;
    emit centerChanged(m_center_tile);
}

void ShadowGenerationController::addTile(std::shared_ptr<Tile> tile) {
    m_tile_heights.insert({tile->id, tile->height_map});
    m_ortho_textures.insert({tile->id, tile->orthotexture});
}
