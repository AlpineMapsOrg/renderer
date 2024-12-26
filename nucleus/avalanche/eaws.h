/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Jörg Christian Reiher
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#pragma once
#ifndef EAWS_H
#define EAWS_H
#include <QDate>
#include <QPainter>
#include <extern/tl_expected/include/tl/expected.hpp>
#include <mapbox/vector_tile.hpp>
#include <nucleus/Raster.h>
#include <nucleus/avalanche/ReportLoadService.h>

namespace radix::tile {
struct Id;
}

namespace avalanche::eaws {
class UIntIdManager; // comes from nucleus/avalanche/UIntIdManager.h

struct Region {
public:
    QString id = ""; // The id is the name of the region e.g. "AT-05-18"
    std::optional<QString> id_alt = std::nullopt; // So far it is not clear what id_alt of an EAWS Region is
    std::optional<QDate> start_date = std::nullopt; // The day the region was defined
    std::optional<QDate> end_date = std::nullopt; // Only for outdated regions: The day the regions expired
    std::vector<glm::vec2> vertices_in_local_coordinates
        = std::vector<glm::vec2>(); // The vertices of the region's bounding polygon with respect to tile resolution, must be in range [0,1]
    glm::uvec2 resolution = glm::vec2(4096, 4096); // Tile resolution
};
using RegionTile = std::pair<radix::tile::Id, std::vector<Region>>;

/* Reads all EAWS regions stored in a provided vector tile
 * Returns a vector of structs, each containing the name, geometry and "alt-id", "start_date", "end_date" if applicable.
 * The mvt file is expected to contain three layers: "micro-regions", "micro-regions_elevation", "outline".
 * The first one is relevant for this class.
 * The parsed layer "micro-regions" contains several features, each feature representing one micro region.
 * Every feature contains at least one poperty and one geometry, the latter one holding the vertices of the region's boundary polygon.
 * Every Property has an id. There must be at least one poperty with its id holding the name-string of the region (e.g. "FR-64").
 * Further properties can exist with ids (all string) "alt-id", "start_date", "end_date".
 * If a region has the property with id "end_date" this region is outdated and was substituted by other regions.
 * These old regions are kept in the data set to allow processing of older bulletins that were using these regions.
 *
 * @param input_data: An array holding the data read froma vector tile (usually obtained by reading a from a mvt file).
 * @param tile_id: The zoom, x-y-cordinates and tile-scheme belonging to the input data
 */
tl::expected<RegionTile, QString> vector_tile_reader(const QByteArray& input_data, const radix::tile::Id& tile_id);

// This struct contains report data written to ubo on gpu
struct uboEawsReports {
    glm::uvec4 reports[1000]; // ~600 regions where each region has a forecast of the form: .x: forecast available (1/0) .y: border .z: rating below border .a: rating above
};

// Creates a new QImage and draws all regions to it where color encodes the region id. Throws error when no regions are provided
// Note: tile_id_out must have greater or equal zoomlevel than tile_id_in
QImage draw_regions(const RegionTile& region_tile,
    std::shared_ptr<UIntIdManager> internal_id_manager,
    const uint& image_width,
    const uint& image_height,
    const radix::tile::Id& tile_id_out,
    const QImage::Format& image_format = QImage::Format_ARGB32);

// Creates a raster from a QImage with regions in it. Throws error when raster_width or raster_height is 0.
// Note: tile_id_out must have greater or equal zoomlevel than tile_id_in
nucleus::Raster<uint16_t> rasterize_regions(
    const RegionTile& region_tile, std::shared_ptr<avalanche::eaws::UIntIdManager> internal_id_manager, const uint raster_width, const uint raster_height, const radix::tile::Id& tile_id_out);

// Overload: Output has same resolution as EAWS regions, throws error when regions.size() == 0
nucleus::Raster<uint16_t> rasterize_regions(const RegionTile& region_tile, std::shared_ptr<avalanche::eaws::UIntIdManager> internal_id_manager);
} // namespace avalanche::eaws
#endif // EAWS_H
