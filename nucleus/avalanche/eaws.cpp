#include "eaws.h"
#include "../vector_tiles/readers.h"
#include "nucleus/utils/tile_conversion.h"
avalanche::eaws::UIntIdManager::UIntIdManager(const QByteArray& vector_tile_data_at_level_0)
{
    // Get Vector of all regions from a tile at level 0
    tl::expected<std::vector<avalanche::eaws::Region>, QString> result = vector_tile::reader::eaws_regions(vector_tile_data_at_level_0, glm::uvec3(0, 0, 0));
    std::vector<avalanche::eaws::Region> eaws_regions;
    if (!result.has_value())
        qFatal("ERROR: constructing avalanche::eaws::RegionIdToColorConverter could not read tile data");

    eaws_regions = result.value();

    // Check if provided vector tail contains at least 611 regions since this is as of June 2024 the minimim amount of regions in a tile of zoom level 0
    if (eaws_regions.size() < 611)
        qFatal("ERROR: constructing avalanche::eaws::RegionIdToColorConverter with tile that is not on zoom level 0 (has less than 611 regions)");

    // Assign internal ids to region ids where 0 means "no region"
    region_id_to_internal_id[QString("")] = 0;
    for (const avalanche::eaws::Region& region : eaws_regions) {
        max_internal_id++;
        region_id_to_internal_id[region.id] = max_internal_id;
    }

    // create inverse map
    for (auto& x : region_id_to_internal_id)
        internal_id_to_region_id[x.second] = x.first;
}

uint avalanche::eaws::UIntIdManager::convert_region_id_to_internal_id(const QString& region_id)
{
    // If Key exists returns its values otherwise create it and return created value
    auto entry = region_id_to_internal_id.find(region_id);
    if (entry == region_id_to_internal_id.end()) {
        max_internal_id++;
        region_id_to_internal_id[region_id] = max_internal_id;
        return max_internal_id;
    } else
        return entry->second;
}

QString avalanche::eaws::UIntIdManager::convert_internal_id_to_region_id(const uint& internal_id) const { return internal_id_to_region_id.at(internal_id); }

QColor avalanche::eaws::UIntIdManager::convert_region_id_to_color(const QString& region_id, QImage::Format color_format)
{
    assert(this->checkIfImageFormatSupported(color_format));
    const uint& internal_id = this->convert_region_id_to_internal_id(region_id);
    assert(internal_id != 0);
    uint red = internal_id / 256;
    uint green = internal_id % 256;
    return QColor::fromRgb(red, green, 0);
}

QString avalanche::eaws::UIntIdManager::convert_color_to_region_id(const QColor& color, const QImage::Format& color_format) const
{
    assert(QImage::Format_ARGB32 == color_format);
    uint internal_id = color.red() * 256 + color.green();
    return internal_id_to_region_id.at(internal_id);
}

uint avalanche::eaws::UIntIdManager::convert_color_to_internal_id(const QColor& color, const QImage::Format& color_format)
{
    return this->convert_region_id_to_internal_id(this->convert_color_to_region_id(color, color_format));
}

QColor avalanche::eaws::UIntIdManager::convert_internal_id_to_color(const uint& internal_id, const QImage::Format& color_format)
{
    return this->convert_region_id_to_color(this->convert_internal_id_to_region_id(internal_id), color_format);
}

bool avalanche::eaws::UIntIdManager::checkIfImageFormatSupported(const QImage::Format& color_format) const
{
    for (const auto& supported_format : this->supported_image_formats) {
        if (color_format == supported_format)
            return true;
    }
    return false;
}

// Draws an EAWS Region to a QImage
void avalanche::eaws::draw_region(
    const Region& region, avalanche::eaws::UIntIdManager& internal_id_manager, QImage* input_img, const glm::uvec3& output_tile_parameters)
{
    assert(input_img->devicePixelRatio() == 1.0);
    //  Input img must have correct resolution and format (format is checked inside convert function)
    assert((region.resolution.x > 0 && region.resolution.y > 0));
    assert((input_img->width() > 0 && input_img->height() > 0));

    // Tile parameters must be consistent
    assert(region.tile_parameters[1] < qPow(2, region.tile_parameters[0]));
    assert(region.tile_parameters[2] < qPow(2, region.tile_parameters[0]));
    assert(output_tile_parameters[1] < qPow(2, output_tile_parameters[0]));
    assert(output_tile_parameters[2] < qPow(2, output_tile_parameters[0]));

    // Create painter on the provided image
    QPainter painter(input_img);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Check whether we are zooming in or out for the output raster
    uint zoom_in = region.tile_parameters[0];
    uint zoom_out = output_tile_parameters[0];
    float tile_size_in = qPow(0.5, zoom_in);
    float tile_size_out = qPow(0.5, zoom_out);
    glm::vec2 origin_in = tile_size_in * glm::vec2((float)region.tile_parameters[1], (float)region.tile_parameters[2]);
    glm::vec2 origin_out = tile_size_out * glm::vec2((float)output_tile_parameters[1], (float)output_tile_parameters[2]);
    float scale_factor = 1.0;
    if (zoom_in <= zoom_out) {

        // Output tile origin must lie within input tile
        assert(origin_in.x <= origin_out.x && origin_in.y <= origin_out.y);
        assert(origin_out.x + tile_size_out <= origin_in.x + tile_size_in && origin_out.y + tile_size_out <= origin_in.y + tile_size_in);

        // Determine origin of output tile w.r.t to input tile. Result in [0,1]x[0,1] since the output origin lies within the input tile
        glm::vec2 origin_out_relative = glm::vec2((origin_out.x - origin_in.x) / tile_size_in, (origin_out.y - origin_in.y) / tile_size_in);

        // Set logical coordinates to the resolution of the region. These are the coordinates within which we provide vertices of region boundaries
        float n = zoom_out - zoom_in; // n is the differnc ein zoom steps between in and out
        float half_pow_n = qPow(0.5, (float)n);
        painter.setWindow(QRect(origin_out_relative.x * input_img->width(), origin_out_relative.y * input_img->height(), half_pow_n * input_img->width(),
            half_pow_n * input_img->height()));
    } else {     
        // zoom_in > zoom_out => Output tile origin must lie within input tile
        assert(origin_out.x <= origin_in.x && origin_out.y <= origin_in.y);
        assert(origin_in.x + tile_size_in <= origin_out.x + tile_size_out && origin_in.y + tile_size_in <= origin_out.y + tile_size_out);

        // Determine origin of input tile w.r.t to output tile. Results in [0,1]x[0,1] since the input origin must lie within the ouput tile
        glm::vec2 origin_in_relative = glm::vec2((origin_in.x - origin_out.x) / tile_size_out, (origin_in.y - origin_out.y) / tile_size_out);

        // Set logical coordinates to the resolution of the region. These are the coordinates within which we provide vertices of region boundaries
        float n = zoom_in - zoom_out; // n is the differnc ein zoom steps between in and out
        float half_pow_n = qPow(0.5, (float)n);
        painter.setWindow(QRect(0, 0, half_pow_n * input_img->width(), half_pow_n * input_img->height()));

        // Set viewport (output image) to the dimensions of the provided image. Painter will now linearly transform logical coords to viewport coords
        painter.setViewport(origin_in_relative.x * input_img->width(), origin_in_relative.y * input_img->height(), half_pow_n * input_img->width(),
            half_pow_n * input_img->height());

        // Set scale factor for the polygon vertices
        scale_factor = half_pow_n;
    }

    // Convert region id to color, for debugging use  color_of_region = QColor::fromRgb(255, 255, 255);
    QColor color_of_region = internal_id_manager.convert_region_id_to_color(region.id, input_img->format());
    painter.setBrush(QBrush(color_of_region));
    painter.setPen(QPen(color_of_region)); // we also have to set the pen if we want to draw boundaries

    // Convert Vertices to QPoints
    std::vector<QPointF> vertices_as_QPointFs;
    for (const glm::vec2& vec : region.vertices_in_local_coordinates) {
        QPointF point(qreal(std::max(0.f, scale_factor * input_img->width() * vec.x)), qreal(std::max(0.f, scale_factor * input_img->height() * vec.y)));
        vertices_as_QPointFs.push_back(point);
    }

    // Draw polygon: The first point is implicitly connected to the last point, and the polygon is filled with the current brush().
    painter.drawPolygon(vertices_as_QPointFs.data(), vertices_as_QPointFs.size());
    assert(input_img->devicePixelRatio() == 1.0);
}

void avalanche::eaws::draw_regions(
    const std::vector<Region>& regions, avalanche::eaws::UIntIdManager& internal_id_manager, QImage* input_img, const glm::uvec3& tile_parameters)
{
    assert(regions.size() > 0);
    for (auto& region : regions) {
        // all regions must lie within the same tile!
        assert(region.tile_parameters == regions[0].tile_parameters);
        avalanche::eaws::draw_region(region, internal_id_manager, input_img, tile_parameters);
    }
}

QImage avalanche::eaws::draw_regions(const std::vector<Region>& regions, avalanche::eaws::UIntIdManager& internal_id_manager, const uint& image_width,
    const uint& image_height, const glm::uvec3& tile_parameters, const QImage::Format& image_format)
{
    // Create correctly formatted image to draw to
    QImage img(image_width, image_height, image_format);
    img.fill(QColor::fromRgb(0, 0, 0));

    // Draw all regions to image
    avalanche::eaws::draw_regions(regions, internal_id_manager, &img, tile_parameters);

    return img;
}

nucleus::Raster<uint16_t> avalanche::eaws::rasterize_regions(const std::vector<avalanche::eaws::Region>& regions,
    avalanche::eaws::UIntIdManager& internal_id_manager, const uint raster_width, const uint raster_height, const glm::uvec3& tile_parameters)
{
    // Draw region ids to image
    const QImage img = draw_regions(regions, internal_id_manager, raster_width, raster_height, tile_parameters);

    // Check if image contains more than one color value
    QRgb color_of_first_pixel = const_cast<QRgb*>(reinterpret_cast<const QRgb*>(img.scanLine(0)))[0];
    bool more_than_one_id_present = false;
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = const_cast<QRgb*>(reinterpret_cast<const QRgb*>(img.scanLine(y)));
        for (int x = 0; x < img.width(); ++x) {
            if (line[x] != color_of_first_pixel) {
                more_than_one_id_present = true;
                break;
            }
        }
        if (more_than_one_id_present)
            break;
    }

    // if only one color value present, return only one pixel with this value
    if (!more_than_one_id_present) {
        QImage onePixel(1, 1, img.format());
        onePixel.fill(color_of_first_pixel);
        return nucleus::utils::tile_conversion::qImage2uint16Raster(onePixel);
    }
    return nucleus::utils::tile_conversion::qImage2uint16Raster(img);
}

nucleus::Raster<uint16_t> avalanche::eaws::rasterize_regions(
    const std::vector<avalanche::eaws::Region>& regions, avalanche::eaws::UIntIdManager& internal_id_manager)
{
    return rasterize_regions(regions, internal_id_manager, regions[0].resolution.x, regions[0].resolution.y, regions[0].tile_parameters);
}
