/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
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

#include <QDebug>
#include <QFile>
#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "nucleus/tile_scheduler/TileLoadService.h"
#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/vector_tiles/VectorTileFeature.h"
#include "nucleus/vector_tiles/VectorTileManager.h"
#include "nucleus/vector_tiles/readers.h"
#include "radix/tile.h"
#include <QPainter>
#include <map>

TEST_CASE("nucleus/vector_tiles")
{
    SECTION("PBF parsing")
    {
        QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, "vectortile.mvt");
        QFile file(filepath);
        file.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QByteArray data = file.readAll();

        CHECK(data.size() > 0);
        const auto id = tile::Id { .zoom_level = 13, .coords = { 4384, 2878 }, .scheme = tile::Scheme::SlippyMap }.to(tile::Scheme::Tms);

        const auto vectortile = nucleus::vectortile::VectorTileManager::to_vector_tile(id, data, nullptr);

        REQUIRE(vectortile->contains(nucleus::vectortile::FeatureType::Peak));

        // std::cout << vectortile.at(nucleus::vectortile::FeatureType::Peak).size() << std::endl;
        CHECK(vectortile->at(nucleus::vectortile::FeatureType::Peak).size() == 16);

        for (const auto& peak : vectortile->at(nucleus::vectortile::FeatureType::Peak)) {

            // std::cout << peak->labelText().toStdString() << " " << peak->id << std::endl;
            // std::cout << peak->position.x << ", " << peak->position.y << std::endl;

            if (peak->id == 26863041ul) {
                // Check if großglockner has been successfully parsed (note the id might change)
                CHECK(peak->name == "Großglockner");

                CHECK(peak->labelText().toStdU16String() == u"Großglockner (3798m)");
            }
        }
    }

    SECTION("Tile download")
    {
        const auto id = tile::Id { .zoom_level = 13, .coords = { 4384, 2878 }, .scheme = tile::Scheme::SlippyMap }.to(tile::Scheme::Tms);

        nucleus::tile_scheduler::TileLoadService service(
            nucleus::vectortile::VectorTileManager::TILE_SERVER, nucleus::tile_scheduler::TileLoadService::UrlPattern::ZXY_yPointingSouth, ".mvt");
        // std::cout << "loading: " << qUtf8Printable(service.build_tile_url(id)) << std::endl;
        {
            QSignalSpy spy(&service, &nucleus::tile_scheduler::TileLoadService::load_finished);
            service.load(id);
            spy.wait(10000);

            REQUIRE(spy.count() == 1);
            QList<QVariant> arguments = spy.takeFirst();
            REQUIRE(arguments.size() == 1);
            nucleus::tile_scheduler::tile_types::TileLayer tile = arguments.at(0).value<nucleus::tile_scheduler::tile_types::TileLayer>();
            CHECK(tile.id == id);
            CHECK(tile.network_info.status == nucleus::tile_scheduler::tile_types::NetworkInfo::Status::Good);
            CHECK(nucleus::tile_scheduler::utils::time_since_epoch() - tile.network_info.timestamp < 10'000);

            QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, "vectortile.mvt");
            QFile file(filepath);
            file.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
            QByteArray data = file.readAll();

            REQUIRE(tile.data->size() > 0);
            CHECK(tile.data->size() == data.size());
            CHECK(*tile.data == data);
        }
    }

    SECTION("Tile download parent")
    {
        nucleus::tile_scheduler::TileLoadService service(
            nucleus::vectortile::VectorTileManager::TILE_SERVER, nucleus::tile_scheduler::TileLoadService::UrlPattern::ZXY_yPointingSouth, ".mvt");

        // tile that contains großglockner
        auto id = tile::Id { .zoom_level = 14, .coords = { 8769, 5757 }, .scheme = tile::Scheme::SlippyMap }.to(tile::Scheme::Tms);

        std::unordered_map<int, size_t> peaks_per_zoom = { { 14, 10 }, { 13, 16 }, { 12, 32 }, { 11, 35 }, { 10, 35 }, { 9, 36 } };

        for (int i = 14; i > 8; i--) {
            // std::cout << "loading: " << i << ": " << qUtf8Printable(service.build_tile_url(id)) << std::endl;

            QSignalSpy spy(&service, &nucleus::tile_scheduler::TileLoadService::load_finished);
            service.load(id);
            spy.wait(10000);

            REQUIRE(spy.count() == 1);
            QList<QVariant> arguments = spy.takeFirst();
            REQUIRE(arguments.size() == 1);
            const auto tile = arguments.at(0).value<nucleus::tile_scheduler::tile_types::TileLayer>();
            CHECK(tile.id == id);
            CHECK(tile.network_info.status == nucleus::tile_scheduler::tile_types::NetworkInfo::Status::Good);
            CHECK(nucleus::tile_scheduler::utils::time_since_epoch() - tile.network_info.timestamp < 10'000);

            REQUIRE(tile.data->size() > 0);

            const auto vectortile = nucleus::vectortile::VectorTileManager::to_vector_tile(id, *tile.data, nullptr);

            REQUIRE(vectortile->contains(nucleus::vectortile::FeatureType::Peak));

            // std::cout << vectortile.at(nucleus::vectortile::FeatureType::Peak).size() << std::endl;

            CHECK(vectortile->at(nucleus::vectortile::FeatureType::Peak).size() == peaks_per_zoom.at(tile.id.zoom_level));

            bool contains_desired_peak = false;

            for (const auto& peak : vectortile->at(nucleus::vectortile::FeatureType::Peak)) {

                // std::cout << peak->labelText().toStdString() << " " << peak->id << std::endl;
                // std::cout << peak->position.x << ", " << peak->position.y << std::endl;

                if (peak->id == 26863041ul) {
                    contains_desired_peak = true;
                    // Check if großglockner has been successfully parsed (note the id might change)
                    CHECK(peak->name == "Großglockner");

                    CHECK(peak->labelText().toStdU16String() == u"Großglockner (3798m)");
                }
            }

            CHECK(contains_desired_peak);

            id = id.parent();
        }
    }
}
TEST_CASE("nucleus/EAWS Vector Tiles")
{
    // Check if test file can be loaded
    const std::string test_file_name = "eaws_0-0-0.mvt";
    QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, test_file_name.c_str());
    QFile test_file(filepath);
    CHECK(test_file.exists());
    CHECK(test_file.size() > 0);

    // Check if testfile can be opened and read
    test_file.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    QByteArray test_data = test_file.readAll();
    test_file.close();
    CHECK(test_data.size() > 0);

    // Check if testdata can be converted to string
    std::string test_data_as_string = test_data.toStdString();
    CHECK(test_data_as_string.size() > 0);

    // Check if tile loading works and at least one layer is present
    mapbox::vector_tile::buffer tileBuffer(test_data_as_string);
    std::map<std::string, const protozero::data_view> layers = tileBuffer.getLayers();
    CHECK(layers.size() > 0);

    // Check if layer with name "micro-regions" exists
    CHECK(layers.contains("micro-regions"));
    mapbox::vector_tile::layer layer = tileBuffer.getLayer("micro-regions");

    // Check if layer micro-regions has enough regions
    CHECK(layer.featureCount() > 0);

    // Check if extend (tile resolution) is >0
    CHECK(layer.getExtent() > 0);

    // Check if reader returns a std::vector with EAWS regions when reading mvt file
    tl::expected<std::vector<avalanche::eaws::EawsRegion>, QString> result;
    result = vector_tile::reader::eaws_regions(test_data);
    CHECK(result.has_value());

    // Check if EAWS region struct is initialized with empty attributes
    const avalanche::eaws::EawsRegion empty_eaws_region;
    CHECK("" == empty_eaws_region.id);
    CHECK(std::nullopt == empty_eaws_region.id_alt);
    CHECK(std::nullopt == empty_eaws_region.start_date);
    CHECK(std::nullopt == empty_eaws_region.end_date);
    CHECK(empty_eaws_region.vertices_in_local_coordinates.empty());

    // Check for some samples of the returned regions if they have the correct properties
    if (result.has_value()) {
        // Retrieve vector of all eaws regions
        std::vector<avalanche::eaws::EawsRegion> eaws_regions = result.value();

        // Retrieve samples that should have certain properties
        avalanche::eaws::EawsRegion region_with_start_date, region_with_end_date, region_with_id_alt;
        for (const avalanche::eaws::EawsRegion& region : eaws_regions) {
            if ("DE-BY-10" == region.id) {
                region_with_id_alt = region;
                region_with_end_date = region;
            }

            if ("IT-23-AO-22" == region.id)
                region_with_start_date = region;

            if (region_with_start_date.id != "" && region_with_end_date.id != "" && region_with_id_alt.id != "")
                break;
        }

        // Check if sample has correct id
        CHECK("DE-BY-10" == region_with_id_alt.id);

        // Check if sample has correct id_alt
        CHECK("BYALL" == region_with_id_alt.id_alt);

        // Check if sample has correct start date
        CHECK(QDate::fromString(QString("2022-03-04"), "yyyy-MM-dd") == region_with_start_date.start_date);

        // Check if struct regions have correct end date for a sample
        CHECK(QDate::fromString(QString("2021-10-01"), "yyyy-MM-dd") == region_with_end_date.end_date);

        // Check if struct regions have correct boundary vertices for a sample
        std::vector<glm::vec2> vertices = region_with_start_date.vertices_in_local_coordinates;
        glm::uvec2 extent(region_with_start_date.resolution.x, region_with_start_date.resolution.y);
        CHECK(4 == vertices.size());
        if (4 == vertices.size()) {
            CHECK(2128 == (int)(vertices[0].x * extent.x));
            CHECK(1459 == (int)(vertices[0].y * extent.y));
            CHECK(2128 == (int)(vertices[1].x * extent.x));
            CHECK(1461 == (int)(vertices[1].y * extent.y));
            CHECK(2128 == (int)(vertices[3].x * extent.x));
            CHECK(1459 == (int)(vertices[3].y * extent.y));
        }

        // Create a eaws color/id converter from a vector tile at zoom level 0
        avalanche::eaws::UIntIdManager internal_id_manager(test_data);

        // Draw regions to image of lower resolution than vector tile extend
        // Visual comparison using img.save(file_path_test_img) with the vector tile in QGIS or similar is recommended after editing relevant code.
        QImage img = avalanche::eaws::draw_regions(eaws_regions, internal_id_manager, 512, 512);
        CHECK(((uint)img.width() == 512 && (uint)img.height() == 512));

        // Rasterize all regions
        const auto raster = avalanche::eaws::rasterize_regions(eaws_regions, internal_id_manager, region_with_start_date.resolution.x, region_with_start_date.resolution.y);

        // Check if raster has correct size
        CHECK((raster.width() == region_with_start_date.resolution.x && raster.height() == region_with_start_date.resolution.y));

        // Check if raster contains correct internal region-ids at certain pixels
        CHECK(0 == raster.pixel(glm::uvec2(0, 0)));
        CHECK(internal_id_manager.convert_region_id_to_internal_id(region_with_start_date.id) == raster.pixel(glm::vec2(2128, 1459)));

        // Check if raster and image have same values when drawn with same resolution
        img = avalanche::eaws::draw_regions(eaws_regions, internal_id_manager, 4096, 4096);
        for (int i = 0; i < 4096; i++) {
            for (int j = 0; j < 4096; j++) {
                uint id_from_img = internal_id_manager.convert_color_to_internal_id(img.pixel(i, j), QImage::Format_ARGB32);
                uint id_from_raster = raster.pixel(glm::uvec2(i, j));
                CHECK(id_from_img == id_from_raster);
            }
        }
    }
}
