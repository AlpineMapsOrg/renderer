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


/*
 * This class controlls the tile loading for the shadow generation usecase
 *
 * it is initialized with a reference point in WebMercator [EPSG:3857] coordinates and a given zoom level,
 * with that information the tile_id of the tile with the given zoom level containing the reference point is calculated
 *
 */
class ShadowGenerationController : public QObject {
    Q_OBJECT

private:
    tile::Id m_center_tile;
    std::map<tile::Id, Raster<uint16_t>> m_tile_heights;
    std::map<tile::Id, QImage> m_ortho_textures;

public:
    ShadowGenerationController(const glm::vec2& reference_point, unsigned int zoom, tile::Scheme = tile::Scheme::Tms);

    //returns an orthographic projection matrix directly looking at the center tile
    Definition tile_cam() const;

    //@param orbit.x defines it's Azimuth in degrees - with 90° = east, 0° = south, -90° = west
    //@param orbit.y defines it's Altidude in degrees - with 0° = highest point, 90° = right on the edge of the horizon
    //@param range defines how many tiles to the left and right and up and down shall also be visible
    //@returns an orthographic projection matrix which is orbited around the center tile
    Definition sun_cam(const glm::vec2& orbit, const glm::vec2& range, float z_offset = 1000);

    const tile::Id& center_tile() const;
    //retuns the orthophoto of the center tile
    const QImage& ortho_texture() const;

    //advances steps tile from the center tile
    void advance(glm::ivec2 steps);

    void addTile(std::shared_ptr<Tile> tile);

    const Raster<uint16_t>& get_height_map(const tile::Id& tile);

signals:
    void centerChanged(const tile::Id& new_center);

private:
    void get_min_max_center(const tile::Id& tile, float& min, float& max, float& center) const;

};

}

#endif // SHADOWGENERATIONCONTROLLER_H
