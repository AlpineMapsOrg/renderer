/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include "Scheduler.h"

#include <unordered_set>

#include <QBuffer>
#include <QStandardPaths>
#include <QTimer>

#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/utils/tile_conversion.h"
#include "sherpa/quad_tree.h"

using namespace nucleus::tile_scheduler;

Scheduler::Scheduler(QObject* parent)
    : Scheduler { white_jpeg_tile(m_ortho_tile_size), black_png_tile(m_height_tile_size), parent }
{
}

Scheduler::Scheduler(const QByteArray& default_ortho_tile, const QByteArray& default_height_tile, QObject* parent)
    : QObject { parent }
{
    m_update_timer = std::make_unique<QTimer>(this);
    m_update_timer->setSingleShot(true);
    connect(m_update_timer.get(), &QTimer::timeout, this, &Scheduler::send_quad_requests);
    connect(m_update_timer.get(), &QTimer::timeout, this, &Scheduler::update_gpu_quads);

    m_purge_timer = std::make_unique<QTimer>(this);
    m_purge_timer->setSingleShot(true);
    connect(m_purge_timer.get(), &QTimer::timeout, this, &Scheduler::purge_ram_cache);

    m_persist_timer = std::make_unique<QTimer>(this);
    m_persist_timer->setSingleShot(true);
    connect(m_persist_timer.get(), &QTimer::timeout, this, &Scheduler::persist_tiles);

    m_default_ortho_tile = std::make_shared<QByteArray>(default_ortho_tile);
    m_default_height_tile = std::make_shared<QByteArray>(default_height_tile);
    read_disk_cache();
}

Scheduler::~Scheduler() = default;

void Scheduler::update_camera(const camera::Definition& camera)
{
    m_current_camera = camera;
    schedule_update();
}

void Scheduler::receive_quads(const std::vector<tile_types::TileQuad>& new_quads)
{
    m_ram_cache.insert(new_quads);
    schedule_purge();
    schedule_update();
    schedule_persist();
}

void Scheduler::update_gpu_quads()
{
    const auto should_refine = tile_scheduler::utils::refineFunctor(m_current_camera, m_aabb_decorator, m_permissible_screen_space_error, m_ortho_tile_size);
    std::vector<tile_types::GpuTileQuad> new_gpu_quads;
    m_ram_cache.visit([this, &new_gpu_quads, &should_refine](const tile_types::TileQuad& quad) {
        if (!should_refine(quad.id))
            return false;
        if (m_gpu_cached.contains(quad.id))
            return true;

        // create GpuQuad based on cpu quad
        tile_types::GpuTileQuad gpu_quad;
        gpu_quad.id = quad.id;
        for (unsigned i = 0; i < quad.n_tiles; ++i) {
            gpu_quad.tiles[i].id = quad.tiles[i].id;
            gpu_quad.tiles[i].bounds = m_aabb_decorator->aabb(quad.tiles[i].id);

            // unpacking the byte data takes long
            const auto* ortho_data = m_default_ortho_tile.get();
            if (quad.tiles[i].ortho) {
                ortho_data = quad.tiles[i].ortho.get();
            }
            auto ortho = nucleus::utils::tile_conversion::toQImage(*ortho_data);
            gpu_quad.tiles[i].ortho = std::make_shared<QImage>(std::move(ortho));

            const auto* height_data = m_default_height_tile.get();
            if (quad.tiles[i].height) {
                height_data = quad.tiles[i].height.get();
            }
            auto heightraster = nucleus::utils::tile_conversion::qImage2uint16Raster(nucleus::utils::tile_conversion::toQImage(*height_data));
            gpu_quad.tiles[i].height = std::make_shared<nucleus::Raster<uint16_t>>(std::move(heightraster));
        }
        new_gpu_quads.push_back(gpu_quad);
        return true;
    });

    std::vector<tile_types::GpuCacheInfo> tiles_to_put_in_gpu_cache;
    tiles_to_put_in_gpu_cache.reserve(new_gpu_quads.size());
    std::transform(new_gpu_quads.cbegin(), new_gpu_quads.cend(), std::back_inserter(tiles_to_put_in_gpu_cache), [](const tile_types::GpuTileQuad& t) {
        return tile_types::GpuCacheInfo { t.id };
    });
    m_gpu_cached.insert(tiles_to_put_in_gpu_cache);

    m_gpu_cached.visit([&should_refine](const tile_types::GpuCacheInfo& quad) {
        return should_refine(quad.id);
    });

    const auto superfluous_quads = m_gpu_cached.purge();

    // elimitate double entries (happens when the gpu has not enough space for all quads selected above)
    std::unordered_set<tile::Id, tile::Id::Hasher> superfluous_ids;
    superfluous_ids.reserve(superfluous_quads.size());
    for (const auto& quad : superfluous_quads)
        superfluous_ids.insert(quad.id);

    std::erase_if(new_gpu_quads, [&superfluous_ids](const tile_types::GpuTileQuad& quad) {
        if (superfluous_ids.contains(quad.id)) {
            superfluous_ids.erase(quad.id);
            return true;
        }
        return false;
    });

    emit gpu_quads_updated(new_gpu_quads, { superfluous_ids.cbegin(), superfluous_ids.cend() });
}

void Scheduler::send_quad_requests()
{
    auto currently_active_tiles = tiles_for_current_camera_position();
    std::erase_if(currently_active_tiles, [this](const tile::Id& id) {
        return m_ram_cache.contains(id);
    });
    emit quads_requested(currently_active_tiles);
}

void Scheduler::purge_ram_cache()
{
    if (m_ram_cache.n_cached_objects() < unsigned(float(m_ram_quad_limit) * 1.1f))
        return;

    const auto should_refine = tile_scheduler::utils::refineFunctor(m_current_camera, m_aabb_decorator, m_permissible_screen_space_error, m_ortho_tile_size);
    m_ram_cache.visit([&should_refine](const tile_types::TileQuad& quad) {
        return should_refine(quad.id);
    });
    m_ram_cache.purge();
}

void Scheduler::persist_tiles()
{
    const auto base_path = disk_cache_path();
    std::filesystem::remove_all(base_path);
    std::filesystem::create_directories(base_path);

    TileId2DataMap ortho_tiles;
    ortho_tiles.reserve(m_ram_cache.n_cached_objects() * 4llu);
    TileId2DataMap height_tiles;
    height_tiles.reserve(m_ram_cache.n_cached_objects() * 4llu);

    m_ram_cache.visit([&ortho_tiles, &height_tiles](const tile_types::TileQuad& quad) {
        for (unsigned i = 0; i < quad.n_tiles; ++i) {
            const auto& tile = quad.tiles[i];
            if (tile.ortho)
                ortho_tiles[tile.id] = tile.ortho;
            if (tile.height)
                height_tiles[tile.id] = tile.height;
        }
        return true;
    });

    const auto height_path = base_path / "height.alp";
    qDebug("writing tile cache to %s", height_path.c_str());
    utils::write_tile_id_2_data_map(height_tiles, height_path);

    const auto ortho_path = base_path / "ortho.alp";
    qDebug("writing tile cache to %s", ortho_path.c_str());
    utils::write_tile_id_2_data_map(ortho_tiles, ortho_path);
}

void Scheduler::schedule_update()
{
    assert(m_update_timeout < std::numeric_limits<int>::max());
    if (m_enabled && !m_update_timer->isActive())
        m_update_timer->start(int(m_update_timeout));
}

void Scheduler::schedule_purge()
{
    assert(m_purge_timeout < std::numeric_limits<int>::max());
    if (m_enabled && !m_purge_timer->isActive()) {
        m_purge_timer->start(int(m_purge_timeout));
    }
}

void Scheduler::schedule_persist()
{
    assert(m_persist_timeout < std::numeric_limits<int>::max());
    if (!m_persist_timer->isActive()) {
        m_persist_timer->start(int(m_persist_timeout));
    }
}

void Scheduler::read_disk_cache()
{
    const auto base_path = disk_cache_path();
//    qDebug("reading disk cache from: %s", base_path.c_str());
    auto ortho_tiles = utils::read_tile_id_2_data_map(base_path / "ortho.alp");
    auto height_tiles = utils::read_tile_id_2_data_map(base_path / "height.alp");
    if (height_tiles.empty() || ortho_tiles.empty()) {
        height_tiles = {};
        ortho_tiles = {};
    }
    std::unordered_map<tile::Id, tile_types::LayeredTile, tile::Id::Hasher> layered_tiles;
    layered_tiles.reserve(ortho_tiles.size());
    for (const auto& v : ortho_tiles) {
        const auto& id = v.first;
        const auto& ortho = v.second;
        if (!height_tiles.contains(id)) {
            qWarning("inconsistent cache. missing id in height tiles");
            return;
        }
        const auto& height = height_tiles.at(id);
        layered_tiles[id] = tile_types::LayeredTile { id, ortho, height };
    }

    std::unordered_map<tile::Id, tile_types::TileQuad, tile::Id::Hasher> quads;
    for (const auto& v : layered_tiles) {
        auto& quad = quads[v.first.parent()];
        if (quad.n_tiles >= 4) {
            qWarning("inconsistent cache. quad.n_tiles >= 4");
            return;
        }
        quad.id = v.first.parent();
        quad.tiles[quad.n_tiles] = v.second;
        ++quad.n_tiles;
    }

    std::vector<tile_types::TileQuad> quads_vector;
    quads_vector.reserve(quads.size());
    std::transform(quads.cbegin(), quads.cend(), std::back_inserter(quads_vector), [this](const auto& pair) {
        tile_types::TileQuad quad = pair.second;
        const auto required_children = quad.id.children();
        std::unordered_set<tile::Id, tile::Id::Hasher> missing_children = { required_children.begin(), required_children.end() };
        for (unsigned i = 0; i < quad.n_tiles; ++i) {
            missing_children.erase(quad.tiles[i].id);
        }
        for (const auto& missing_id : missing_children) {
            quad.tiles[quad.n_tiles].id = missing_id;
            quad.tiles[quad.n_tiles].ortho = m_default_ortho_tile;
            quad.tiles[quad.n_tiles].height = m_default_height_tile;
            quad.n_tiles++;
        }
        return quad;
    });
    receive_quads(quads_vector);
}

std::vector<tile::Id> Scheduler::tiles_for_current_camera_position() const
{
    std::vector<tile::Id> all_inner_nodes;
    const auto all_leaves = quad_tree::onTheFlyTraverse(
        tile::Id { 0, { 0, 0 } },
        tile_scheduler::utils::refineFunctor(m_current_camera, m_aabb_decorator, m_permissible_screen_space_error, m_ortho_tile_size),
        [&all_inner_nodes](const tile::Id& v) { all_inner_nodes.push_back(v); return v.children(); });

    //    all_inner_nodes.reserve(all_inner_nodes.size() + all_leaves.size());
    //    std::copy(all_leaves.begin(), all_leaves.end(), std::back_inserter(all_inner_nodes));
    return all_inner_nodes;
}

unsigned int Scheduler::persist_timeout() const
{
    return m_persist_timeout;
}

void Scheduler::set_persist_timeout(unsigned int new_persist_timeout)
{
    assert(new_persist_timeout < std::numeric_limits<int>::max());
    m_persist_timeout = new_persist_timeout;

    if (m_persist_timer->isActive()) {
        m_persist_timer->start(int(m_persist_timeout));
    }
}

const Cache<tile_types::TileQuad>& Scheduler::ram_cache() const
{
    return m_ram_cache;
}

QByteArray Scheduler::white_jpeg_tile(unsigned int size)
{
    QImage default_tile(QSize { int(size), int(size) }, QImage::Format_ARGB32);
    default_tile.fill(Qt::GlobalColor::white);
    QByteArray arr;
    QBuffer buffer(&arr);
    buffer.open(QIODevice::WriteOnly);
    default_tile.save(&buffer, "JPEG");
    return arr;
}

QByteArray Scheduler::black_png_tile(unsigned size)
{
    QImage default_tile(QSize { int(size), int(size) }, QImage::Format_ARGB32);
    default_tile.fill(Qt::GlobalColor::black);
    QByteArray arr;
    QBuffer buffer(&arr);
    buffer.open(QIODevice::WriteOnly);
    default_tile.save(&buffer, "PNG");
    return arr;
}

std::filesystem::path Scheduler::disk_cache_path()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toStdString() + "/tile_cache";
}

void Scheduler::set_purge_timeout(unsigned int new_purge_timeout)
{
    assert(new_purge_timeout < std::numeric_limits<int>::max());
    m_purge_timeout = new_purge_timeout;

    if (m_purge_timer->isActive()) {
        m_purge_timer->start(int(m_update_timeout));
    }
}

void Scheduler::set_ram_quad_limit(unsigned int new_ram_quad_limit)
{
    m_ram_quad_limit = new_ram_quad_limit;
    m_ram_cache.set_capacity(new_ram_quad_limit);
}

void Scheduler::set_gpu_quad_limit(unsigned int new_gpu_quad_limit)
{
    m_gpu_quad_limit = new_gpu_quad_limit;
    m_gpu_cached.set_capacity(m_gpu_quad_limit);
}

void Scheduler::set_aabb_decorator(const utils::AabbDecoratorPtr& new_aabb_decorator)
{
    m_aabb_decorator = new_aabb_decorator;
}

void Scheduler::set_permissible_screen_space_error(float new_permissible_screen_space_error)
{
    m_permissible_screen_space_error = new_permissible_screen_space_error;
}

bool Scheduler::enabled() const
{
    return m_enabled;
}

void Scheduler::set_enabled(bool new_enabled)
{
    m_enabled = new_enabled;
    schedule_update();
}

void Scheduler::set_update_timeout(unsigned new_update_timeout)
{
    assert(m_update_timeout < std::numeric_limits<int>::max());
    m_update_timeout = new_update_timeout;
    if (m_update_timer->isActive()) {
        m_update_timer->start(m_update_timeout);
    }
}
