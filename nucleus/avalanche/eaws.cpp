#include "eaws.h"
#include "nucleus/utils/tile_conversion.h"

// Auxillary class for parsing mapbox vector tiles recommend by mapbox to be used for this purpose
class StringPrintVisitor {
public:
    QString operator()(std::vector<mapbox::feature::value>) { return QStringLiteral("vector"); }
    QString operator()(std::unordered_map<std::string, mapbox::feature::value>) { return QStringLiteral("unordered_map"); }
    QString operator()(mapbox::feature::null_value_t) { return QStringLiteral("null"); }
    QString operator()(std::nullptr_t) { return QStringLiteral("nullptr"); }
    QString operator()(uint64_t val) { return QString::number(val); }
    QString operator()(int64_t val) { return QString::number(val); }
    QString operator()(double val) { return QString::number(val); }
    QString operator()(std::string const& val) { return QString::fromStdString(val); }
    QString operator()(bool val) { return val ? QStringLiteral("true") : QStringLiteral("false"); }
};

tl::expected<std::vector<avalanche::eaws::Region>, QString> avalanche::eaws::vector_tile_reader(const QByteArray& input_data)
{
    // This name could theoretically be changed by the EAWS (very unlikely though)
    const QString& name_of_layer_with_eaws_regions = "micro-regions";

    // Get all layers of the vector tile and check that the relevant layer (containing EAWS micro regions) exists
    std::string input_data_as_string = input_data.toStdString();
    mapbox::vector_tile::buffer tile_buffer(input_data_as_string);
    std::map<std::string, const protozero::data_view> layers = tile_buffer.getLayers();
    if (!layers.contains(name_of_layer_with_eaws_regions.toStdString())) {
        QString error_message
            = "ERROR in vector_tile::reader::eaws_region: The vector tile contains no layer with name " + name_of_layer_with_eaws_regions + ".";
        return tl::unexpected(error_message);
    }

    // Get the relevant layer and check if it contains data
    mapbox::vector_tile::layer layer = tile_buffer.getLayer(name_of_layer_with_eaws_regions.toStdString());
    if (layer.featureCount() <= 0) {
        QString error_message = "ERROR in vector_tile::reader::eaws_region: The vector tile contains no EAWS micro-regions in the layer \""
            + name_of_layer_with_eaws_regions + "\".";
        return tl::unexpected(error_message);
    }

    // Ensure extend > 0. Extend is an integer representing the resolution of the square tile. Usually the extend is 4096
    if (layer.getExtent() <= 0) {
        QString error_message = "ERROR in vector_tile::reader::eaws_region: Vector tile has extend <= 0.";
        return tl::unexpected(error_message);
    }
    uint extent = layer.getExtent();

    // Loop through features = micro-regions of the layer
    std::vector<avalanche::eaws::Region> regions_to_be_returned;
    for (std::size_t feature_index = 0; feature_index < layer.featureCount(); feature_index++) {
        avalanche::eaws::Region region;
        region.resolution = glm::ivec2(extent, extent); // Parse properties of the region (name, start date, end date)
        const protozero::data_view& feature_data_view = layer.getFeature(feature_index);
        mapbox::vector_tile::feature current_feature(feature_data_view, layer);
        mapbox::vector_tile::feature::properties_type properties = current_feature.getProperties();
        for (const auto& property : properties) {
            StringPrintVisitor string_print_visitor;
            QString value = mapbox::util::apply_visitor(string_print_visitor, property.second);
            if ("id" == property.first)
                region.id = value;
            else if ("id_alt" == property.first)
                region.id_alt = value;
            else if ("start_date" == property.first)
                region.start_date = QDate::fromString(value, "yyyy-MM-dd");
            else if ("end_date" == property.first)
                region.end_date = QDate::fromString(value, "yyyy-MM-dd");
        }

        // Parse geometry (boundary vertices) of current region
        mapbox::vector_tile::points_arrays_type geometry = current_feature.getGeometries<mapbox::vector_tile::points_arrays_type>(1.0);
        for (const auto& point_array : geometry) {
            for (const auto& point : point_array) {
                region.vertices_in_local_coordinates.push_back(glm::vec2(point.x, point.y) / (float)layer.getExtent());
            }
        }

        // Add current region to vector of regions
        regions_to_be_returned.push_back(region);
    }
    return tl::expected<std::vector<avalanche::eaws::Region>, QString>(regions_to_be_returned);
}

avalanche::eaws::UIntIdManager::UIntIdManager(const QByteArray& vector_tile_data_at_level_0)
{
    // Get Vector of all regions from a tile at level 0
    tl::expected<std::vector<avalanche::eaws::Region>, QString> result = avalanche::eaws::vector_tile_reader(vector_tile_data_at_level_0);
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

// Auxillary function: Calculates new coordinates of a region boundary after zoom in / out
std::vector<QPointF> transform_vertices(const avalanche::eaws::Region& region, const tile::Id& tile_id_in, const tile::Id& tile_id_out, QImage* img)
{
    // Check if input is consistent
    assert(img->devicePixelRatio() == 1.0);
    assert((region.resolution.x > 0 && region.resolution.y > 0));
    assert((img->width() > 0 && img->height() > 0));
    assert(tile_id_in.coords.x < qPow(2, tile_id_in.zoom_level));
    assert(tile_id_in.coords.y < qPow(2, tile_id_in.zoom_level));
    assert(tile_id_out.coords.x < qPow(2, tile_id_out.zoom_level));
    assert(tile_id_out.coords.y < qPow(2, tile_id_out.zoom_level));

    // Check whether we are zooming in or out for the output raster
    uint zoom_in = tile_id_in.zoom_level;
    uint zoom_out = tile_id_out.zoom_level;
    float tile_size_in = qPow(0.5f, zoom_in);
    float tile_size_out = qPow(0.5f, zoom_out);
    glm::vec2 origin_in = tile_size_in * glm::vec2((float)tile_id_in.coords.x, (float)tile_id_in.coords.y);
    glm::vec2 origin_out = tile_size_out * glm::vec2((float)tile_id_out.coords.x, (float)tile_id_out.coords.y);
    float relative_zoom = 1.f;
    glm::vec2 relative_origin(0.f, 0.f);

    if (zoom_in < zoom_out) {

        // Output tile origin must lie within input tile
        assert(origin_in.x <= origin_out.x && origin_in.y <= origin_out.y);
        assert(origin_out.x + tile_size_out <= origin_in.x + tile_size_in && origin_out.y + tile_size_out <= origin_in.y + tile_size_in);

        // Determine origin of output tile w.r.t to input tile. Result in [0,1]x[0,1] since the output origin lies within the input tile
        relative_origin = glm::vec2((origin_out.x - origin_in.x) / tile_size_in, (origin_out.y - origin_in.y) / tile_size_in);

        // Calculate scale factor
        float n = zoom_out - zoom_in; // n is the differnc ein zoom steps between in and out
        relative_zoom = qPow(2, (float)n);

    } else if (zoom_out < zoom_in) {
        // zoom_in > zoom_out => Output tile origin must lie within input tile
        assert(origin_out.x <= origin_in.x && origin_out.y <= origin_in.y);
        assert(origin_in.x + tile_size_in <= origin_out.x + tile_size_out && origin_in.y + tile_size_in <= origin_out.y + tile_size_out);

        // Determine origin of input tile w.r.t to output tile. Results in [0,1]x[0,1] since the input origin must lie within the ouput tile
        relative_origin = glm::vec2((origin_in.x - origin_out.x) / tile_size_out, (origin_in.y - origin_out.y) / tile_size_out);

        // Set logical coordinates to the resolution of the region. These are the coordinates within which we provide vertices of region boundaries
        float n = zoom_in - zoom_out; // n is the differnc ein zoom steps between in and out
        relative_zoom = qPow(0.5, (float)n);
    }

    // Transform boundary according to input/output tile parameters
    std::vector<QPointF> transformed_vertices_as_QPointFs;
    QTransform trafo
        = QTransform::fromTranslate(relative_origin.x, relative_origin.y) * QTransform::fromScale(img->width() * relative_zoom, img->height() * relative_zoom);
    for (const glm::vec2& vec : region.vertices_in_local_coordinates)
        transformed_vertices_as_QPointFs.push_back(trafo.map(QPointF((float)vec.x, (float)vec.y)));

    // Return transformed vertices
    return transformed_vertices_as_QPointFs;
}

QImage avalanche::eaws::draw_regions(const RegionTile& region_tile, avalanche::eaws::UIntIdManager* internal_id_manager, const uint& image_width,
    const uint& image_height, const tile::Id& tile_id_out, const QImage::Format& image_format)
{
    // Create correctly formatted image to draw to
    QImage img(image_width, image_height, image_format);
    img.fill(QColor::fromRgb(0, 0, 0));

    // Create painter on the provided image
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Draw all regions to the image
    assert(region_tile.second.size() > 0);
    tile::Id tile_id_in = region_tile.first;
    for (const auto& region : region_tile.second) {
        // Calculate vertex coordinates of region w.r.t. output tile at output resolution
        std::vector<QPointF> transformed_vertices_as_QPointFs = transform_vertices(region, tile_id_in, tile_id_out, &img);

        // Convert region id to color, for debugging use  color_of_region = QColor::fromRgb(255, 255, 255);
        QColor color_of_region = internal_id_manager->convert_region_id_to_color(region.id, img.format());
        painter.setBrush(QBrush(color_of_region));
        painter.setPen(QPen(color_of_region)); // we also have to set the pen if we want to draw boundaries

        // Draw polygon: The first point is implicitly connected to the last point, and the polygon is filled with the current brush().
        assert(img.devicePixelRatio() == 1.0);
        painter.drawPolygon(transformed_vertices_as_QPointFs.data(), transformed_vertices_as_QPointFs.size());
        assert(img.devicePixelRatio() == 1.0);
    }
    // return image with all regions in it
    return img;
}

nucleus::Raster<uint16_t> avalanche::eaws::rasterize_regions(const RegionTile& region_tile, avalanche::eaws::UIntIdManager* internal_id_manager,
    const uint raster_width, const uint raster_height, const tile::Id& tile_id_out)
{
    // Draw region ids to image
    const QImage img = draw_regions(region_tile, internal_id_manager, raster_width, raster_height, tile_id_out);

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

nucleus::Raster<uint16_t> avalanche::eaws::rasterize_regions(const RegionTile& region_tile, avalanche::eaws::UIntIdManager* internal_id_manager)
{
    return rasterize_regions(region_tile, internal_id_manager, region_tile.second[0].resolution.x, region_tile.second[0].resolution.y, region_tile.first);
}
