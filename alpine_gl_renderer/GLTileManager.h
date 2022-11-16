/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include <QObject>

#include "alpine_gl_renderer/GLTileSet.h"
#include "alpine_gl_renderer/GLVariableLocations.h"
#include "nucleus/Tile.h"

namespace camera {
class Definition;
}

class QOpenGLShaderProgram;

class GLTileManager : public QObject {
    Q_OBJECT
public:
    explicit GLTileManager(QObject* parent = nullptr);

    [[nodiscard]] const std::vector<GLTileSet>& tiles() const;
    void draw(QOpenGLShaderProgram* shader_program, const camera::Definition& camera) const;

signals:
    void tilesChanged();

public slots:
    void addTile(const std::shared_ptr<Tile>& tile);
    void removeTile(const tile::Id& tile_id);
    void setAttributeLocations(const TileGLAttributeLocations& d);
    void setUniformLocations(const TileGLUniformLocations& d);

private:
    static constexpr auto N_EDGE_VERTICES = 65;
    static constexpr auto MAX_TILES_PER_TILESET = 1;
    float m_max_anisotropy = 0;

    std::vector<GLTileSet> m_gpu_tiles;
    // indexbuffers for 4^index tiles,
    // e.g., for single tile tile sets take index 0
    //       for 4 tiles take index 1, for 16 2..
    // the size_t is the number of indices
    std::vector<std::pair<std::unique_ptr<QOpenGLBuffer>, size_t>> m_index_buffers;
    TileGLAttributeLocations m_attribute_locations;
    TileGLUniformLocations m_uniform_locations;
    unsigned m_tiles_per_set = 1;
};
