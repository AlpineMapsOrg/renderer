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

    SECTION("EAWS Vector Tiles")
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
            std::vector<glm::ivec2> vertices = region_with_start_date.vertices_in_local_coordinates;
            CHECK(4 == vertices.size());
            if (4 == vertices.size()) {
                CHECK(2128 == vertices[0].x);
                CHECK(1459 == vertices[0].y);
                CHECK(2128 == vertices[1].x);
                CHECK(1461 == vertices[1].y);
                CHECK(2128 == vertices[3].x);
                CHECK(1459 == vertices[3].y);
            }
            //-----------------------------------------------------------------------------------------------
            // Sort Region Ids starting at 1 such that 0 means no region
            /*
            std::map<std::string, uint> get_internal_id_from_region_name;
            for (const avalanche::eaws::EawsRegion& region : eaws_regions) {
                get_internal_id_from_region_name[region.id.toStdString()] = 0;
            }
            std::map<uint, std::string> get_region_name_from_internal_id;
            uint i = 1;
            for (auto& x : get_internal_id_from_region_name) {
                x.second = i;
                get_region_name_from_internal_id[i] = x.first;
                i++;
            }
            */

            std::map<std::string, uint> get_internal_id_from_region_name;
            std::map<uint, std::string> get_region_name_from_internal_id;
            std::tie(get_internal_id_from_region_name, get_region_name_from_internal_id) = avalanche::eaws::create_internal_ids(eaws_regions);

            // Setup QPainter and QImage for drawing polygons
            uint res_x = region_with_start_date.resolution.x;
            uint res_y = region_with_start_date.resolution.y;
            QImage img(res_x, res_y, QImage::Format_RGB888);
            img.fill(QColor::fromRgb(0, 0, 0));
            QPainter painter(&img);

            // Draw regions to image
            for (const avalanche::eaws::EawsRegion& region : eaws_regions) {

                // Setup color code from region id
                uint idNumber = get_internal_id_from_region_name[region.id.toStdString()];
                uint redValue = idNumber / 256;
                uint greenValue = idNumber % 256;
                QColor colorOfRegion = QColor::fromRgb(redValue, greenValue, 0);
                painter.setBrush(QBrush(colorOfRegion));
                painter.setPen(QPen(colorOfRegion)); // we also have to set the pen to draw the boundaries
                std::vector<glm::ivec2> vertices = region.vertices_in_local_coordinates;

                // Convert Vertices to QPoints
                std::vector<QPointF> vertices_as_QPointFs;
                for (glm::ivec2& vec : vertices) {
                    QPointF point(qreal(std::max(0, vec.x)), qreal(std::max(0, vec.y)));
                    vertices_as_QPointFs.push_back(point);
                }

                // Draw polygon: The first point is implicitly connected to the last point, and the polygon is filled with the current brush().
                painter.drawPolygon(vertices_as_QPointFs.data(), vertices_as_QPointFs.size());
            }

            // Save image
            const std::string test_image_file_name = "C:\\Users\\JCR\\jcrTest.bmp";
            // QString filepathTestImage = QString("%1%2").arg(ALP_TEST_DATA_DIR, test_image_file_name.c_str());
            bool it_worked = img.save(QString::fromStdString(test_image_file_name));
            if (it_worked)
                std::cout << "\n SAVED IMAGE\n";
            else
                std::cout << "\n NOT SAVED IMAGE\n";

            // Debug:
            std::cout << "\n##########################################################################";
            std::cout << "\nimg[0,0] = " << img.pixelColor(0, 0).red() << img.pixelColor(0, 0).green();
            std::cout << "\nimg[2128,1459] = " << img.pixelColor(2128, 1459).red() << img.pixelColor(2128, 1459).green();
            std::cout << "\n~ " << get_region_name_from_internal_id[img.pixelColor(2128, 1459).red() * 256 + img.pixelColor(2128, 1459).green()];
        }
    }
}
