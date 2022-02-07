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

#include "GLTileManager.h"

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>

#include "render_backend/utils/terrain_mesh_index_generator.h"

#include <render_backend/Tile.h>

namespace {
template <typename T>
int bufferLengthInBytes(const std::vector<T>& vec) {
  return int(vec.size() * sizeof(T));
}

std::vector<QVector4D> boundsArray(const GLTileSet& tileset) {
  std::vector<QVector4D> ret;
  ret.reserve(tileset.tiles.size());
  for (const auto& tile : tileset.tiles) {
    ret.emplace_back(tile.second.min.x,
                     tile.second.min.y,
                     tile.second.max.x,
                     tile.second.max.y);
  }
  return ret;
}
}

GLTileManager::GLTileManager(QObject *parent)
    : QObject{parent}
{
  for (auto i = 0; i < MAX_TILES_PER_TILESET; ++i) {
    const auto indices = terrain_mesh_index_generator::surface_quads<uint16_t>(N_EDGE_VERTICES);
    auto index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    index_buffer->create();
    index_buffer->bind();
    index_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    index_buffer->allocate(indices.data(), bufferLengthInBytes(indices));
    index_buffer->release();
    m_index_buffers.emplace_back(std::move(index_buffer), indices.size());
  }
}

const std::vector<GLTileSet>& GLTileManager::tiles() const
{
  return m_gpu_tiles;
}

void GLTileManager::draw(QOpenGLShaderProgram* shader_program) const
{
  QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();
  shader_program->setUniformValue(m_uniform_locations.n_edge_vertices, N_EDGE_VERTICES);
  for (const auto& tileset : tiles()) {
    tileset.vao->bind();
    const auto bounds = boundsArray(tileset);
    shader_program->setUniformValueArray(m_uniform_locations.bounds_array, bounds.data(), int(bounds.size()));
    f->glDrawElements(GL_TRIANGLE_STRIP, tileset.gl_element_count,  tileset.gl_index_type, nullptr);
  }
  f->glBindVertexArray(0);
}

void GLTileManager::addTile(const std::shared_ptr<Tile>& tile)
{
  assert(m_attribute_locations.height != -1);
  auto* f = QOpenGLContext::currentContext()->extraFunctions();
  // need to call GLWindow::makeCurrent, when calling through signals?
  // find an empty slot => todo, for now just create a new tile every time.
  // setup / copy data to gpu
  GLTileSet tileset;
  tileset.tiles.emplace_back(tile->id, tile->bounds);
  tileset.vao = std::make_unique<QOpenGLVertexArrayObject>();
  tileset.vao->create();
  tileset.vao->bind();
  {   // vao state
    tileset.heightmap_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    tileset.heightmap_buffer->create();
    tileset.heightmap_buffer->bind();
    tileset.heightmap_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    tileset.heightmap_buffer->allocate(tile->height_map.buffer().data(), bufferLengthInBytes(tile->height_map.buffer()));
    f->glEnableVertexAttribArray(GLuint(m_attribute_locations.height));
    f->glVertexAttribPointer(GLuint(m_attribute_locations.height), /*size*/ 1, /*type*/ GL_UNSIGNED_SHORT, /*normalised*/ GL_TRUE, /*stride*/ 0, nullptr);

    m_index_buffers[0].first->bind();
    tileset.gl_element_count = int(m_index_buffers[0].second);
    tileset.gl_index_type = GL_UNSIGNED_SHORT;
  }
  tileset.vao->release();

  // add to m_gpu_tiles
  m_gpu_tiles.push_back(std::move(tileset));

  emit tilesChanged();
}

void GLTileManager::removeTile(const srs::TileId& tile_id)
{
  // clear slot
  // or remove from list and free resources

  emit tilesChanged();
}

void GLTileManager::setAttributeLocations(const TileGLAttributeLocations& d)
{
  m_attribute_locations = d;
}

void GLTileManager::setUniformLocations(const TileGLUniformLocations& d)
{
  m_uniform_locations = d;
}
