#ifndef EAWS_H
#define EAWS_H
#include "nucleus/Raster.h"
#include <QDate>
#include <QPainter>
#include <QString>
#include <extern/tl_expected/include/tl/expected.hpp>
#include <glm/glm.hpp>
#include <optional>
namespace avalanche::eaws {
struct Region {
public:
    glm::uvec3 tile_parameters; // Components: [zoom-level, x-index, y-index]
    QString id = ""; // The id is the name of the region e.g. "AT-05-18"
    std::optional<QString> id_alt = std::nullopt; // So far it is not clear what id_alt of an EAWS Region is
    std::optional<QDate> start_date = std::nullopt; // The day the region was defined
    std::optional<QDate> end_date = std::nullopt; // Only for outdated regions: The day the regions expired
    std::vector<glm::vec2> vertices_in_local_coordinates
        = std::vector<glm::vec2>(); // The vertices of the region's bounding polygon with respect to tile resolution, must be in range [0,1]
    glm::uvec2 resolution = glm::vec2(4096, 4096); // Tile resolution
};

// This class handles conversion from region-id strings to internal ids as uint and as color
// It must be e constructed from an eaws vector tile at level 0 to extract all region names
class UIntIdManager {
public:
    const std::vector<QImage::Format> supported_image_formats { QImage::Format_ARGB32 };
    UIntIdManager(const QByteArray& vector_tile_data_at_level_0);
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

// Draws an EAWS Region to a provided QImage that must have the right format.
void draw_region(const Region& region, avalanche::eaws::UIntIdManager& internal_id_manager, QImage* img, const glm::uvec3& tile_parameters);

// Writes all regions to provided QImage. Regions must all have same zoom level. Throws error when regions.size() == 0
void draw_regions(const std::vector<Region>& regions, avalanche::eaws::UIntIdManager& internal_id_manager, QImage* img, const glm::uvec3& tile_parameters);

// Creates a new QImage and writes all regions to it. Throws error when regions.size() == 0
QImage draw_regions(const std::vector<Region>& regions, avalanche::eaws::UIntIdManager& internal_id_manager, const uint& image_width, const uint& image_height,
    const glm::uvec3& tile_parameters, const QImage::Format& image_format = QImage::Format_ARGB32);

// Output has custom resolution,  throws error when raster_width or raster_height is 0.
nucleus::Raster<uint16_t> rasterize_regions(const std::vector<Region>& regions, avalanche::eaws::UIntIdManager& internal_id_manager, const uint raster_width,
    const uint raster_height, const glm::uvec3& tile_parameters);

// output has same resolution as EAWS regions, throws error when regions.size() == 0
nucleus::Raster<uint16_t> rasterize_regions(const std::vector<Region>& regions, avalanche::eaws::UIntIdManager& internal_id_manager);
} // namespace avalanche::eaws
#endif // EAWS_H
