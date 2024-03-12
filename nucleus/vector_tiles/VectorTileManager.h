/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
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

#pragma once

#include <mapbox/vector_tile.hpp>

#include <memory>
#include <string>
#include <unordered_map>

#include <QByteArray>
#include <QObject>

// #include "nucleus/DataQuerier.h"
#include "nucleus/vector_tiles/VectorTileFeature.h"

namespace nucleus::vectortile {

class VectorTileManager : public QObject {
    Q_OBJECT
public:
    explicit VectorTileManager(QObject* parent = nullptr);

    inline static const QString TILE_SERVER = "http://localhost:8080/austria.peaks/";

    static const VectorTile toVectorTile(const QByteArray& vectorTileData);

private:
    inline static std::unordered_map<unsigned long, std::shared_ptr<FeatureTXT>> m_loaded_features = {};

    // all individual features and an appropriate parser method are stored in the following map
    // typedef std::shared_ptr<FeatureTXT> (*FeatureTXTParser)(const mapbox::vector_tile::feature& feature, tile::SrsBounds& tile_bounds, double extent);
    typedef std::shared_ptr<FeatureTXT> (*FeatureTXTParser)(const mapbox::vector_tile::feature&);
    inline static const std::unordered_map<std::string, FeatureTXTParser> FEATURE_TYPES_FACTORY = { { "Peak", FeatureTXTPeak::parse } };
    inline static const std::unordered_map<std::string, FeatureType> FEATURE_TYPES = { { "Peak", FeatureType::Peak } };
};

} // namespace nucleus::vectortile
