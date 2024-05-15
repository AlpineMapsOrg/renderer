#include "readers.h"

// This is an auxillary class for parsing mapbox vector tiles recommend by mapbox to be used for this purpose
class PrintValue {
public:
    std::string operator()(std::vector<mapbox::feature::value>) { return "vector"; }
    std::string operator()(std::unordered_map<std::string, mapbox::feature::value>) { return "unordered_map"; }
    std::string operator()(mapbox::feature::null_value_t) { return "null"; }
    std::string operator()(std::nullptr_t) { return "nullptr"; }
    std::string operator()(uint64_t val) { return std::to_string(val); }
    std::string operator()(int64_t val) { return std::to_string(val); }
    std::string operator()(double val) { return std::to_string(val); }
    std::string operator()(std::string const& val) { return val; }
    std::string operator()(bool val) { return val ? "true" : "false"; }
};

tl::expected<std::vector<avalanche::eaws::EawsRegion>, QString> vector_tile::reader::eaws_regions(const QByteArray& input_data)
{
    const QString& name_of_layer_with_eaws_regions = "micro-regions";
    // Get all layers of the vector tile and check that the relevant layer (containing EAWS micro regions) exists
    std::string input_data_as_string = input_data.toStdString();
    mapbox::vector_tile::buffer tile_buffer(input_data_as_string);
    std::map<std::string, const protozero::data_view> layers = tile_buffer.getLayers();
    if (layers.size() <= 0)
        return tl::unexpected("\nERROR in vector_tile::reader::eaws_region: Vector tile contains no layers");

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

    // Loop through features = micro-regions of the layer
    std::vector<avalanche::eaws::EawsRegion> regions_to_be_returned;
    for (std::size_t feature_index = 0; feature_index < layer.featureCount(); feature_index++) {
        avalanche::eaws::EawsRegion region;

        // Parse properties of the region (name, start date, end date)
        const protozero::data_view& feature_data_view = layer.getFeature(feature_index);
        mapbox::vector_tile::feature current_feature(feature_data_view, layer);
        mapbox::vector_tile::feature::properties_type properties = current_feature.getProperties();
        for (const auto& property : properties) {
            PrintValue print_visitor;
            std::string value = mapbox::util::apply_visitor(print_visitor, property.second);
            if ("id" == property.first)
                region.id = QString::fromStdString(value);
            else if ("id_alt" == property.first)
                region.id_alt = QString::fromStdString(value);
            else if ("start_date" == property.first)
                region.start_date = QString::fromStdString(value);
            else if ("end_date" == property.first)
                region.end_date = QString::fromStdString(value);
        }

        // Parse geometry (boundary vertices) of current region
        mapbox::vector_tile::points_arrays_type geometry = current_feature.getGeometries<mapbox::vector_tile::points_arrays_type>(1.0);
        for (const auto& point_array : geometry) {
            for (const auto& point : point_array) {
                region.vertices_in_local_coordinates.push_back(glm::ivec2(point.x, point.y));
            }
        }

        // Add current region to vector of regions
        regions_to_be_returned.push_back(region);
    }
    return tl::expected<std::vector<avalanche::eaws::EawsRegion>, QString>(regions_to_be_returned);
}
