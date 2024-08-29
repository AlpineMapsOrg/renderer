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


#include <memory>
#include <string>
#include <unordered_map>
#include <set>

#include <QByteArray>
#include <QObject>
#include <mapbox/vector_tile.hpp>
#include <radix/tile.h>

#include "feature.h"

namespace nucleus {
class DataQuerier;
}

namespace nucleus::vectortile {

class VectorTileManager : public QObject {
    Q_OBJECT
public:
    explicit VectorTileManager(QObject* parent = nullptr);

    static const QString tile_server();

    static const std::shared_ptr<FeatureSet> to_vector_tile(const QByteArray& vectorTileData, const std::shared_ptr<DataQuerier> dataquerier);

private:
    // all individual features and an appropriate parser method are stored in the following map
    typedef std::shared_ptr<const FeatureTXT> (*FeatureTXTParser)(const mapbox::vector_tile::feature&, const std::shared_ptr<DataQuerier>);
    inline static const std::unordered_map<std::string, FeatureTXTParser> m_feature_types_factory = { { "Peak", FeatureTXTPeak::parse },
        { "cities", FeatureTXTCity::parse }, { "cottages", FeatureTXTCottage::parse }, { "webcams", FeatureTXTWebcam::parse } };

    //    inline static const QString m_tile_server = "http://localhost:3000/poi_v1/";
    inline static const QString m_tile_server = "https://osm.cg.tuwien.ac.at/vector_tiles/poi_v1/";

    // contains all chars that were encountered while parsing vector tiles
    // this is later used in the get_diff_chars method to find characters which still have to be rendered
    inline static std::set<char16_t> m_all_chars;
};

} // namespace nucleus::vectortile
