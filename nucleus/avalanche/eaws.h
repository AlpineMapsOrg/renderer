#ifndef EAWS_H
#define EAWS_H
#include <QDate>
#include <QPainter>
#include <extern/tl_expected/include/tl/expected.hpp>
#include <mapbox/vector_tile.hpp>
#include <nucleus/Raster.h>
#include <radix/tile.h>

namespace avalanche::eaws {
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
using RegionTile = std::pair<tile::Id, std::vector<Region>>;

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
tl::expected<RegionTile, QString> vector_tile_reader(const QByteArray& input_data, const tile::Id& tile_id);

// This class handles conversion from region-id strings to internal ids as uint and as color
class UIntIdManager {
public:
    const std::vector<QImage::Format> supported_image_formats { QImage::Format_ARGB32 };
    UIntIdManager();
    QColor convert_region_id_to_color(const QString& region_id, QImage::Format color_format = QImage::Format_ARGB32);
    QString convert_color_to_region_id(const QColor& color, const QImage::Format& color_format) const;
    uint convert_region_id_to_internal_id(const QString& color);
    QString convert_internal_id_to_region_id(const uint& internal_id) const;
    uint convert_color_to_internal_id(const QColor& color, const QImage::Format& color_format);
    QColor convert_internal_id_to_color(const uint& internal_id, const QImage::Format& color_format);
    bool checkIfImageFormatSupported(const QImage::Format& color_format) const;

private:
    std::unordered_map<QString, uint> region_id_to_internal_id;
    std::unordered_map<uint, QString> internal_id_to_region_id;
    uint max_internal_id = 0;
};

// Creates a new QImage and draws all regions to it where color encodes the region id. Throws error when no regions are provided
QImage draw_regions(const RegionTile& region_tile, avalanche::eaws::UIntIdManager* internal_id_manager, const uint& image_width, const uint& image_height,
    const tile::Id& tile_id_out, const QImage::Format& image_format = QImage::Format_ARGB32);

// Creates a raster from a QImage with regions in it. Throws error when raster_width or raster_height is 0.
nucleus::Raster<uint16_t> rasterize_regions(const RegionTile& region_tile, avalanche::eaws::UIntIdManager* internal_id_manager, const uint raster_width,
    const uint raster_height, const tile::Id& tile_id_out);

// Overload: Output has same resolution as EAWS regions, throws error when regions.size() == 0
nucleus::Raster<uint16_t> rasterize_regions(const RegionTile& region_tile, avalanche::eaws::UIntIdManager* internal_id_manager);
} // namespace avalanche::eaws
#endif // EAWS_H
