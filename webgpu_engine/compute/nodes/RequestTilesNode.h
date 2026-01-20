/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include "Node.h"
#include "nucleus/tile/TileLoadService.h"

namespace webgpu_engine::compute::nodes {

class RequestTilesNode : public Node {
    Q_OBJECT

public:
    struct RequestTilesNodeSettings {
        std::string tile_path = "https://alpinemaps.cg.tuwien.ac.at/tiles/at_dtm_alpinemaps/";
        nucleus::tile::TileLoadService::UrlPattern url_pattern = nucleus::tile::TileLoadService::UrlPattern::ZXY;
        std::string file_extension = ".png";
    };

    RequestTilesNode();
    RequestTilesNode(const RequestTilesNodeSettings& settings);

    void on_single_tile_received(const nucleus::tile::Data& tile);

    void set_settings(const RequestTilesNodeSettings& settings);

    void check_progress_and_emit_signals();

public slots:
    void run_impl() override;

private:
    RequestTilesNodeSettings m_settings;
    std::unique_ptr<nucleus::tile::TileLoadService> m_tile_loader;
    size_t m_num_signals_received = 0;
    size_t m_num_tiles_unavailable = 0;
    size_t m_num_tiles_requested = 0;
    std::vector<QByteArray> m_received_tile_textures;
    std::vector<radix::tile::Id> m_requested_tile_ids;
};

} // namespace webgpu_engine::compute::nodes
