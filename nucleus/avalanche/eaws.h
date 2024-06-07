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
struct EawsRegion {
public:
    QString id = ""; // The id is the name of the region e.g. "AT-05-18"
    std::optional<QString> id_alt = std::nullopt; // So far it is not clear what id_alt of an EAWS Region is
    std::optional<QDate> start_date = std::nullopt; // The day the region was defined
    std::optional<QDate> end_date = std::nullopt; // Only for outdated regions: The day the regions expired
    std::vector<glm::ivec2> vertices_in_local_coordinates; // The vertices of the region's bounding polygon with respect to tile resolution
    static constexpr glm::ivec2 resolution = glm::vec2(4096, 4096); // Tile resolution
};

// This class handles conversion from region-id strings to internal ids as uint and as color
// It must be e constructed from an eaws vector tile at level 0 to extract all region names
class InternalIdManager {
public:
    const std::vector<QImage::Format> supported_image_formats { QImage::Format_RGB888 };
    InternalIdManager(const QByteArray& vector_tile_data_at_level_0);
    QColor convert_region_id_to_color(const QString& region_id, QImage::Format color_format = QImage::Format_RGB888) const;
    QString convert_color_to_region_id(const QColor& color, const QImage::Format& color_format) const;
    uint convert_region_id_to_internal_id(const QString& color) const;
    QString convert_internal_id_to_region_id(const uint& internal_id) const;
    uint convert_color_to_internal_id(const QColor& color, const QImage::Format& color_format) const;
    QColor convert_internal_id_to_color(const uint& internal_id, const QImage::Format& color_format) const;
    bool checkIfImageFormatSupported(const QImage::Format& color_format) const;

private:
    std::unordered_map<QString, uint> region_id_to_internal_id;
    std::unordered_map<uint, QString> internal_id_to_region_id;
};

// Draws an EAWS Region to a provided QImage that must have the right format.
QImage draw_region(const EawsRegion& region, const avalanche::eaws::InternalIdManager& internal_id_manager, const QImage& img);

// Writes all regions to provided QImage. Regions must all have same zoom level. Throws error when regions.size() == 0
QImage draw_regions(const std::vector<EawsRegion>& regions, const avalanche::eaws::InternalIdManager& internal_id_manager, const QImage& img);

// Creates a new QImage and writes all regions to it. Throws error when regions.size() == 0
QImage draw_regions(const std::vector<EawsRegion>& regions, const avalanche::eaws::InternalIdManager& internal_id_manager,
    const QImage::Format& image_format = QImage::Format_RGB888);

// Throws error when regions.size() == 0
nucleus::Raster<uint> rasterize_regions(const std::vector<EawsRegion>& regions, const avalanche::eaws::InternalIdManager& internal_id_manager);
} // namespace avalanche::eaws
#endif // EAWS_H
