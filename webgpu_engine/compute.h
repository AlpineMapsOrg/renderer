/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "nucleus/tile_scheduler/TileLoadService.h"
#include "nucleus/tile_scheduler/tile_types.h"
#include "raii/Texture.h"
#include <QObject>
#include <vector>

namespace webgpu_engine {

struct RectangularTileRegion {
    glm::uvec2 min;
    glm::uvec2 max;
    unsigned int zoom_level;
    tile::Scheme scheme;

    std::vector<tile::Id> get_tiles() const;
};

class ComputeTileStorage {
public:
    virtual ~ComputeTileStorage() = default;
    virtual void init() = 0;
    virtual void store(const tile::Id& id, std::shared_ptr<QByteArray> data) = 0;
    virtual void clear(const tile::Id& id) = 0;
};

class TextureArrayComputeTileStorage : public ComputeTileStorage {
public:
    TextureArrayComputeTileStorage(size_t n_edge_vertices, size_t capacity);

    void init() override;
    void store(const tile::Id& id, std::shared_ptr<QByteArray> data) override;
    void clear(const tile::Id& id) override;

private:
    [[maybe_unused]]size_t m_n_edge_vertices;   // TODO: use
    [[maybe_unused]]size_t m_capacity;          // TODO: use
    std::unique_ptr<raii::Texture> m_texture_array;
    std::vector<tile::Id> m_layer_index_to_tile_id;
};

class ComputeController : public QObject {
    Q_OBJECT

public:
    ComputeController();
    ~ComputeController() = default;

    void request_and_store_tiles(const RectangularTileRegion& region);
    void run_pipeline();

public slots:
    void on_single_tile_received(const nucleus::tile_scheduler::tile_types::TileLayer& tile);
    void on_all_tiles_received();

private:
    std::unique_ptr<nucleus::tile_scheduler::TileLoadService> m_tile_loader;
    std::unique_ptr<ComputeTileStorage> m_tile_storage;
    const size_t m_max_num_tiles = 256;
    const size_t m_num_edge_vertices = 65;
    size_t m_num_tiles_received = 0;
    size_t m_num_tiles_requested = 0;
};

} // namespace webgpu_engine
