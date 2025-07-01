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

#include "eaws.h"
#include "UIntIdManager.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <algorithm>
#include <extern/radix/src/radix/tile.h>
#include <nucleus/tile/conversion.h>
#include <nucleus/vector_tile/util.h>

namespace nucleus::avalanche {

tl::expected<RegionTile, QString> vector_tile_reader(const QByteArray& input_data, const radix::tile::Id& tile_id)
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
    std::vector<Region> regions_to_be_returned;
    for (std::size_t feature_index = 0; feature_index < layer.featureCount(); feature_index++) {
        Region region;
        region.resolution = glm::ivec2(extent, extent); // Parse properties of the region (name, start date, end date)
        const protozero::data_view& feature_data_view = layer.getFeature(feature_index);
        mapbox::vector_tile::feature current_feature(feature_data_view, layer);
        mapbox::vector_tile::feature::properties_type properties = current_feature.getProperties();
        for (const auto& property : properties) {
            QString value = std::visit(nucleus::vector_tile::util::string_print_visitor, property.second);
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

    // Combine all regions with their tile id and return this pair
    return tl::expected<RegionTile, QString>(RegionTile(tile_id, regions_to_be_returned));
}

// Auxillary function: Calculates new coordinates of a region boundary after zoom in / out
std::vector<QPointF> transform_vertices(const Region& region, const radix::tile::Id& tile_id_in, const radix::tile::Id& tile_id_out, QImage* img)
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
    uint zoom_level_in = tile_id_in.zoom_level;
    uint zoom_level_out = tile_id_out.zoom_level;
    float tile_size_in = qPow(0.5f, zoom_level_in);
    float tile_size_out = qPow(0.5f, zoom_level_out);
    glm::vec2 origin_in = tile_size_in * glm::vec2((float)tile_id_in.coords.x, (float)tile_id_in.coords.y);
    glm::vec2 origin_out = tile_size_out * glm::vec2((float)tile_id_out.coords.x, (float)tile_id_out.coords.y);
    float relative_zoom = 1.f;
    glm::vec2 relative_origin(0.f, 0.f);

    if (zoom_level_in < zoom_level_out) {

        // Output tile origin must lie within input tile
        assert(origin_in.x <= origin_out.x && origin_in.y <= origin_out.y);
        assert(origin_out.x + tile_size_out <= origin_in.x + tile_size_in && origin_out.y + tile_size_out <= origin_in.y + tile_size_in);

        // Determine origin of output tile w.r.t to input tile. Result in [0,1]x[0,1] since the output origin lies within the input tile
        relative_origin = glm::vec2((origin_out.x - origin_in.x) / tile_size_in, (origin_out.y - origin_in.y) / tile_size_in);

        // Calculate scale factor
        float n = zoom_level_out - zoom_level_in; // n is the differnc ein zoom steps between in and out
        relative_zoom = qPow(2, (float)n);
    }

    // This case does not work and it is not clear at this point if this case is necessary
    assert(zoom_level_in <= zoom_level_out);
    /*
    else if (zoom_level_out < zoom_level_in) {
        // zoom_in > zoom_out => Output tile origin must lie within input tile
        assert(origin_out.x <= origin_in.x && origin_out.y <= origin_in.y);
        assert(origin_in.x + tile_size_in <= origin_out.x + tile_size_out && origin_in.y + tile_size_in <= origin_out.y + tile_size_out);

        // Determine origin of input tile w.r.t to output tile. Results in [0,1]x[0,1] since the input origin must lie within the ouput tile
        relative_origin = glm::vec2((origin_in.x - origin_out.x) / tile_size_out, (origin_in.y - origin_out.y) / tile_size_out);

        // Set logical coordinates to the resolution of the region. These are the coordinates within which we provide vertices of region boundaries
        float n = zoom_level_in - zoom_level_out; // n is the differnc ein zoom steps between in and out
        relative_zoom = qPow(0.5, (float)n);
    }
    */

    // Transform boundary according to input/output tile parameters
    std::vector<QPointF> transformed_vertices_as_QPointFs;
    for (const glm::vec2& vec_in : region.vertices_in_local_coordinates) {
        glm::vec2 vec_out = (vec_in - relative_origin) * relative_zoom;
        transformed_vertices_as_QPointFs.push_back(QPointF((float)vec_out.x * img->width(), (float)vec_out.y * img->height()));
    }

    // Return transformed vertices
    return transformed_vertices_as_QPointFs;
}

// Auxillaryfunctions: Checks if a region is valid for the currently selected eaws report
bool region_matches_report_date(const Region& region, const QDate& report_date)
{
    // report dates lies before validity range of region >> return false
    if (region.start_date != std::nullopt) {
        if (report_date < region.start_date)
            return false;
    }

    // report dates lies after validity range of region >> return false
    if (region.end_date != std::nullopt) {
        if (region.end_date < report_date)
            return false;
    }

    // report dates lies inside validity range of region >> return true
    return true;
}

QImage draw_regions(const RegionTile& region_tile,
    std::shared_ptr<UIntIdManager> internal_id_manager,
    const uint& image_width,
    const uint& image_height,
    const radix::tile::Id& tile_id_out,
    const QImage::Format& image_format)
{
    // Create correctly formatted image to draw to
    QImage img(image_width, image_height, image_format);
    img.fill(QColor::fromRgb(0, 0, 0));

    // Create painter on the provided image
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Draw all regions to the image
    assert(region_tile.second.size() > 0);
    radix::tile::Id tile_id_in = region_tile.first;
    for (const auto& region : region_tile.second) {
        // Only draw regions that match current eaws report
        if (!region_matches_report_date(region, internal_id_manager->get_date()))
            continue;

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

nucleus::Raster<uint16_t> rasterize_regions(const RegionTile& region_tile,
    std::shared_ptr<UIntIdManager> internal_id_manager,
    const uint raster_width,
    const uint raster_height,
    const radix::tile::Id& tile_id_out)
{
    // Draw region ids to image, if all pixel have same value return one pixel with this value
    const QImage img = draw_regions(region_tile, internal_id_manager, raster_width, raster_height, tile_id_out);
    const auto raster = nucleus::tile::conversion::qimage_to_u16raster(img);
    const auto first_pixel = raster.pixel({ 0, 0 });
    for (auto p : raster) {
        if (p != first_pixel)
            return raster;
    }
    return nucleus::Raster<uint16_t>({ 1, 1 }, first_pixel);
}

nucleus::Raster<uint16_t> rasterize_regions(const RegionTile& region_tile, std::shared_ptr<UIntIdManager> internal_id_manager)
{
    return rasterize_regions(region_tile, internal_id_manager, region_tile.second[0].resolution.x, region_tile.second[0].resolution.y, region_tile.first);
}

} // namespace nucleus::avalanche
