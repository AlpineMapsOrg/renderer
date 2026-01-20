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

#include "RequestTilesNode.h"

#include <QDebug>
#include <set>

namespace webgpu_engine::compute::nodes {

RequestTilesNode::RequestTilesNode(const RequestTilesNodeSettings& settings)
    : Node(
          {
              InputSocket(*this, "tile ids", data_type<const std::vector<radix::tile::Id>*>()),
          },
          {
              OutputSocket(*this, "tile data", data_type<const std::vector<QByteArray>*>(), [this]() { return &m_received_tile_textures; }),
          })
{
    set_settings(settings);
}

RequestTilesNode::RequestTilesNode()
    : RequestTilesNode(RequestTilesNodeSettings())
{
}

void RequestTilesNode::run_impl()
{
    qDebug() << "running HeightRequestNode ...";

    // get tile ids to request
    // TODO maybe make get_input_data a template (so usage would become get_input_data<type>(socket_index))
    const auto& tile_ids = *std::get<data_type<const std::vector<radix::tile::Id>*>()>(input_socket("tile ids").get_connected_data());

    // if input tile ids didnt change from last run
    std::set<radix::tile::Id> new_tile_ids(tile_ids.begin(), tile_ids.end());
    std::set<radix::tile::Id> old_tile_ids(m_requested_tile_ids.begin(), m_requested_tile_ids.end());
    if (new_tile_ids == old_tile_ids) {
        qDebug() << "tiles already requested, use cache";
        check_progress_and_emit_signals();
        return;
    }

    // send request for each tile
    m_received_tile_textures.resize(tile_ids.size());
    m_requested_tile_ids = tile_ids;
    m_num_tiles_requested = m_received_tile_textures.size();
    m_num_tiles_unavailable = 0;
    m_num_signals_received = 0;
    qDebug() << "requesting " << m_num_tiles_requested << " tiles ...";
    for (const auto& tile_id : tile_ids) {
        m_tile_loader->load(tile_id);
    }
}

void RequestTilesNode::on_single_tile_received(const nucleus::tile::Data& tile)
{
    auto found_it = std::find(m_requested_tile_ids.begin(), m_requested_tile_ids.end(), tile.id);
    if (found_it == m_requested_tile_ids.end()) {
        // received tile id that was not requested
        // this means, we send requested a new set of tiles before the responses for the old ones arrived
        // ignore those, we are only interested in responses to the last set of requested tiles
        return;
    }

    m_num_signals_received++;
    if (tile.network_info.status != nucleus::tile::NetworkInfo::Status::Good) {
        m_num_tiles_unavailable++;
        qWarning() << "failed to load tile id x=" << tile.id.coords.x << ", y=" << tile.id.coords.y << ", zoomlevel=" << tile.id.zoom_level << ": "
                   << (tile.network_info.status == nucleus::tile::NetworkInfo::Status::NotFound ? "Not found" : "Network error");
    } else {
        size_t found_index = found_it - m_requested_tile_ids.begin();
        m_received_tile_textures[found_index] = *tile.data;
    }

    check_progress_and_emit_signals();
}

void RequestTilesNode::set_settings(const RequestTilesNodeSettings& settings)
{
    m_settings = settings;
    m_tile_loader = std::make_unique<nucleus::tile::TileLoadService>(QString::fromStdString(settings.tile_path), settings.url_pattern, QString::fromStdString(settings.file_extension));
    connect(m_tile_loader.get(), &nucleus::tile::TileLoadService::load_finished, this, &RequestTilesNode::on_single_tile_received);
}

void RequestTilesNode::check_progress_and_emit_signals()
{
    // when all requests are finished (either failed or successfully)
    if (m_num_signals_received == m_num_tiles_requested) {
        if (m_num_tiles_unavailable > 0) {
            emit run_failed(NodeRunFailureInfo(*this, std::format("failed to load {} tiles from {}", m_num_tiles_unavailable, m_settings.tile_path)));
        } else {
            emit run_completed();
        }
    }
}

} // namespace webgpu_engine::compute::nodes
