#ifndef SHADOWGENERATIONCONTROLLER_H
#define SHADOWGENERATIONCONTROLLER_H

#include "Definition.h"
#include "qtmetamacros.h"

#include <sherpa/tile.h>

#include <QObject>
#include <map>

#include <nucleus/Raster.h>
#include <nucleus/Tile.h>


namespace camera{

class ShadowGenerationController : public QObject {
    Q_OBJECT

private:
    tile::Id m_center_tile;
    std::map<tile::Id, Raster<uint16_t>> m_tile_heights;
    std::map<tile::Id, QImage> m_ortho_textures;

public:
    ShadowGenerationController(const glm::vec2& reference_point, unsigned int zoom, tile::Scheme = tile::Scheme::Tms);

    Definition tile_cam() const;
    Definition sun_cam(const glm::vec2& orbit, const glm::vec2& range, float z_offset = 1000);

    const tile::Id& center_tile() const;
    const QImage& ortho_texture() const;

    void advance(glm::ivec2 steps);

    void addTile(std::shared_ptr<Tile> tile);

signals:
    void centerChanged(const tile::Id& new_center);

private:
    void get_min_max_center(const tile::Id& tile, float& min, float& max, float& center) const;

};

}

#endif // SHADOWGENERATIONCONTROLLER_H
