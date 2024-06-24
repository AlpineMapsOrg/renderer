#include "eaws.h"
#include "../vector_tiles/readers.h"
avalanche::eaws::InternalIdManager::InternalIdManager(const QByteArray& vector_tile_data_at_level_0)
{
    // Get Vector of all regions from a tile at level 0
    tl::expected<std::vector<avalanche::eaws::EawsRegion>, QString> result = vector_tile::reader::eaws_regions(vector_tile_data_at_level_0);
    std::vector<avalanche::eaws::EawsRegion> eaws_regions;
    if (!result.has_value())
        qFatal("ERROR: constructing avalanche::eaws::RegionIdToColorConverter could not read tile data");

    eaws_regions = result.value();

    // Check if provided vector tail contains at least 611 regions since this is as of June 2024 the minimim amount of regions in a tile of zoom level 0
    if (eaws_regions.size() < 611)
        qFatal("ERROR: constructing avalanche::eaws::RegionIdToColorConverter with tile that is not on zoom level 0 (has less than 611 regions)");

    // Assign internal ids to region ids where 0 means "no region"
    region_id_to_internal_id[QString("")] = 0;
    uint internal_id = 1;
    for (const avalanche::eaws::EawsRegion& region : eaws_regions) {
        region_id_to_internal_id[region.id] = internal_id;
        internal_id++;
    }

    // create inverse map
    for (auto& x : region_id_to_internal_id)
        internal_id_to_region_id[x.second] = x.first;
}

uint avalanche::eaws::InternalIdManager::convert_region_id_to_internal_id(const QString& region_id) const { return region_id_to_internal_id.at(region_id); }

QString avalanche::eaws::InternalIdManager::convert_internal_id_to_region_id(const uint& internal_id) const { return internal_id_to_region_id.at(internal_id); }

QColor avalanche::eaws::InternalIdManager::convert_region_id_to_color(const QString& region_id, QImage::Format color_format) const
{
    assert(this->checkIfImageFormatSupported(color_format));
    const uint& internal_id = this->region_id_to_internal_id.at(region_id);
    assert(internal_id != 0);
    uint red = internal_id / 256;
    uint green = internal_id % 256;
    return QColor::fromRgb(red, green, 0);
}

QString avalanche::eaws::InternalIdManager::convert_color_to_region_id(const QColor& color, const QImage::Format& color_format) const
{
    assert(QImage::Format_RGB888 == color_format);
    uint internal_id = color.red() * 256 + color.green();
    return internal_id_to_region_id.at(internal_id);
}

uint avalanche::eaws::InternalIdManager::convert_color_to_internal_id(const QColor& color, const QImage::Format& color_format) const
{
    return this->convert_region_id_to_internal_id(this->convert_color_to_region_id(color, color_format));
}

QColor avalanche::eaws::InternalIdManager::convert_internal_id_to_color(const uint& internal_id, const QImage::Format& color_format) const
{
    return this->convert_region_id_to_color(this->convert_internal_id_to_region_id(internal_id), color_format);
}

bool avalanche::eaws::InternalIdManager::checkIfImageFormatSupported(const QImage::Format& color_format) const
{
    for (const auto& supported_format : this->supported_image_formats) {
        if (color_format == supported_format)
            return true;
    }
    return false;
}

// Draws an EAWS Region to a QImage
QImage avalanche::eaws::draw_region(const EawsRegion& region, const avalanche::eaws::InternalIdManager& internal_id_manager, const QImage& input_img)
{
    //  Input img must have correct resolution and format (format is checked inside convert function)
    assert((region.resolution.x > 0 && region.resolution.y > 0));
    assert((input_img.width() > 0 && input_img.height() > 0));

    // Create a copy of the input Image
    QImage img(input_img);
    QPainter painter(&img);

    // Set logical coordinates to the resolution of the region.
    // These are the coordinates within which we provide vertices of region boundaries
    painter.setWindow(QRect(0, 0, region.resolution.x, region.resolution.y));

    // Set viewport (output image) to the dimensions of the provided image. Painter will now linearly transform logical coords to viewport coords
    painter.setViewport(0, 0, img.width(), img.height());

    // Convert region id to color
    QColor color_of_region = internal_id_manager.convert_region_id_to_color(region.id, input_img.format());
    painter.setBrush(QBrush(color_of_region));
    painter.setPen(QPen(color_of_region)); // we also have to set the pen if we want to draw boundaries

    // Convert Vertices to QPoints
    std::vector<QPointF> vertices_as_QPointFs;
    for (const glm::vec2& vec : region.vertices_in_local_coordinates) {
        QPointF point(qreal(std::max(0.f, region.resolution.x * vec.x)), qreal(std::max(0.f, region.resolution.y * vec.y)));
        vertices_as_QPointFs.push_back(point);
    }

    // Draw polygon: The first point is implicitly connected to the last point, and the polygon is filled with the current brush().
    painter.drawPolygon(vertices_as_QPointFs.data(), vertices_as_QPointFs.size());

    // return an image of the regions where each region is filled with its internal id as color code
    return img;
}

QImage avalanche::eaws::draw_regions(const std::vector<EawsRegion>& regions, const avalanche::eaws::InternalIdManager& internal_id_manager, const QImage& input_img)
{
    assert(regions.size() > 0);
    QImage img(input_img);
    for (auto& region : regions) {
        img = avalanche::eaws::draw_region(region, internal_id_manager, img);
    }
    return img;
}

QImage avalanche::eaws::draw_regions(const std::vector<EawsRegion>& regions, const avalanche::eaws::InternalIdManager& internal_id_manager,
    const uint& image_width, const uint& image_height, const QImage::Format& image_format)
{
    // Create correctly formatted image to draw to
    QImage img(image_width, image_height, image_format);

    img.fill(QColor::fromRgb(0, 0, 0));

    // Draw all regions to image
    return avalanche::eaws::draw_regions(regions, internal_id_manager, img);
}

nucleus::Raster<uint> avalanche::eaws::rasterize_regions(const std::vector<avalanche::eaws::EawsRegion>& regions,
    const avalanche::eaws::InternalIdManager& internal_id_manager, const uint raster_width, const uint raster_height)
{
    QImage img = draw_regions(regions, internal_id_manager, raster_width, raster_height);
    nucleus::Raster<uint> raster(img.width());
    for (int i = 0; i < img.width(); i++) {
        for (int j = 0; j < img.width(); j++) {
            raster.pixel(glm::uvec2(i, j)) = internal_id_manager.convert_color_to_internal_id(img.pixelColor(i, j), img.format());
        }
    }
    return raster;
}

nucleus::Raster<uint> avalanche::eaws::rasterize_regions(
    const std::vector<avalanche::eaws::EawsRegion>& regions, const avalanche::eaws::InternalIdManager& internal_id_manager)
{
    QImage img = draw_regions(regions, internal_id_manager, regions[0].resolution.x, regions[0].resolution.y);
    nucleus::Raster<uint> raster(img.width());
    for (int i = 0; i < img.width(); i++) {
        for (int j = 0; j < img.width(); j++) {
            raster.pixel(glm::uvec2(i, j)) = internal_id_manager.convert_color_to_internal_id(img.pixelColor(i, j), img.format());
        }
    }
    return raster;
}
