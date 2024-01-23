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
#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "nucleus/vector_tiles/VectorTileManager.h"
#include "radix/tile.h"

TEST_CASE("nucleus/vector_tiles")
{
    SECTION("PBF parsing")
    {
        nucleus::VectorTileManager v;

        QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, "vector_tile_13_2878_4384.pbf");

        QFile file(filepath);
        file.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QByteArray data = file.readAll();

        CHECK(data.size() > 0);

        tile::Id id(13, glm::uvec2(4384, 2878), tile::Scheme::Tms);

        v.update_tile_data(data.toStdString(), id);

        // TODO teufelskamp appears twice...
        // Teufelskamp	alt:3509	pos: 1.41144e+06, 5.95626e+06
        // Teufelskamp	alt:3511	pos: 1.41144e+06, 5.95626e+06

        for (const auto& peak : v.get_peaks()) {
            std::cout << peak.second->name << "\talt:" << peak.second->altitude << "\tpos: " << peak.second->position.x << ", " << peak.second->position.y << std::endl;
        }

        std::cout << "peaks: " << v.get_peaks().size() << std::endl;
    }

    SECTION("Tile Loading")
    {
        nucleus::VectorTileManager v;
        tile::Id id(13, glm::uvec2(4384, 2878), tile::Scheme::Tms);
        v.get_tile(id);
    }
}
