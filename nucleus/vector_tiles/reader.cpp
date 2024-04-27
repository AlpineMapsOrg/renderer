#include "reader.h"
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

tl::expected<std::vector<EawsRegion>, std::string> vector_tile::reader::eaws_regions(const QByteArray& inputData, const std::string& nameOfLayerWithEawsRegions)
{
    // Get all layers of the vector tile and check that the relevant layer (containing EAWS micro regions) exists
    std::string inputDataAsString = inputData.toStdString();
    mapbox::vector_tile::buffer tileBuffer(inputDataAsString);
    std::map<std::string, const protozero::data_view> layers = tileBuffer.getLayers();
    if (layers.size() <= 0)
        return tl::unexpected("\nERROR in vector_tile::reader::eaws_region: Vector tile contains no layers");

    if (!layers.contains(nameOfLayerWithEawsRegions)) {
        std::string error_message
            = "ERROR in vector_tile::reader::eaws_region: The vector tile contains no layer with name " + nameOfLayerWithEawsRegions + ".";
        if (layers.contains("micro-regions"))
            error_message += "\nNOTE: Layer \"micro-regions\" exists. Maybe try loading this.";
        return tl::unexpected(error_message);
    }

    // Get the relevant layer and check if it contains data
    mapbox::vector_tile::layer layer = tileBuffer.getLayer(nameOfLayerWithEawsRegions);
    if (layer.featureCount() <= 0) {
        std::string error_message
            = "ERROR in vector_tile::reader::eaws_region: The vector tile contains no EAWS micro-regions in the layer \"" + nameOfLayerWithEawsRegions + "\".";
        return tl::unexpected(error_message);
    }

    // Ensure extend > 0. Extend is an integer representing the resolution of the square tile. Usually the extend is 4096
    if (layer.getExtent() <= 0) {
        std::string error_message = "ERROR in vector_tile::reader::eaws_region: Vector tile has extend <= 0.";
        return tl::unexpected(error_message);
    }

    // Loop through features = micro-regions of the layer
    std::vector<EawsRegion> regionsToBeReturned;
    for (std::size_t featureIdx = 0; featureIdx < layer.featureCount(); featureIdx++) {
        EawsRegion region;

        // Parse properties of the region (name, start date, end date)
        protozero::data_view const& featureDataView = layer.getFeature(featureIdx);
        mapbox::vector_tile::feature currentFeature(featureDataView, layer);
        mapbox::vector_tile::feature::properties_type properties = currentFeature.getProperties();
        for (auto const& property : properties) {
            PrintValue printvisitor;
            std::string value = mapbox::util::apply_visitor(printvisitor, property.second);
            if ("id" == property.first)
                region.id = value;
            else if ("id_alt" == property.first)
                region.id_alt = value;
            else if ("start_date" == property.first)
                region.start_date = value;
            else if ("end_date" == property.first)
                region.end_date = value;
        }

        // Parse geometry (boundary vertices) of current region
        mapbox::vector_tile::points_arrays_type geometry = currentFeature.getGeometries<mapbox::vector_tile::points_arrays_type>(1.0);
        for (auto const& point_array : geometry) {
            for (auto const& point : point_array) {
                region.verticesInLocalCoordinates.push_back(glm::ivec2(point.x, point.y));
            }
        }

        // Add current region to vector of regions
        regionsToBeReturned.push_back(region);
    }
    return tl::expected<std::vector<EawsRegion>, std::string>(regionsToBeReturned);
}
