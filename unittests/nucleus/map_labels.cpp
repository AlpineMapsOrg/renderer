/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
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
#include <QImage>
#include <catch2/catch_test_macros.hpp>
#include <nucleus/map_label/Factory.h>
#include <nucleus/tile/conversion.h>

TEST_CASE("nucleus/map_label/factory")
{
    nucleus::map_label::Factory f;
    auto a = f.init_font_atlas();
    auto pois = nucleus::vector_tile::PointOfInterestCollection();
    auto poi = nucleus::vector_tile::PointOfInterest();
    poi.name = "ασδφ";
    pois.emplace_back(poi);
    f.create_labels(pois);
    a = f.renew_font_atlas();
    auto i = 0u;
    for (const auto& rg_raster : a.font_atlas) {
        CAPTURE(rg_raster);
        auto rgba_raster = nucleus::Raster<glm::u8vec4>(rg_raster.size());
        std::transform(rg_raster.begin(), rg_raster.end(), rgba_raster.begin(), [](glm::u8vec2 rg) { return glm::u8vec4 { rg.x, rg.y, 0, 255 }; });
        const auto qimage = nucleus::tile::conversion::to_QImage(rgba_raster);
        qimage.save(QString("font_atlas_%0.png").arg(i));
    }
}
