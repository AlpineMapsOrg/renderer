#include "readers.h"

// This is an auxillary class for parsing mapbox vector tiles recommend by mapbox to be used for this purpose
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

tl::expected<std::vector<avalanche::eaws::Region>, QString> vector_tile::reader::eaws_regions(const QByteArray& input_data, const glm::uvec3& tile_parameters)
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
        avalanche::eaws::Region region = { tile_parameters };
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
