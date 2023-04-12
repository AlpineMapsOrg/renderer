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
#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/utils/tile_conversion.h"
#include "sherpa/quad_tree.h"

#include <QBuffer>
#include <QTimer>
#include <unordered_set>

using namespace nucleus::tile_scheduler;

Scheduler::Scheduler(QObject *parent)
    : QObject{parent}
{
    m_update_timer = std::make_unique<QTimer>();
    m_update_timer->setSingleShot(true);
    connect(m_update_timer.get(), &QTimer::timeout, this, &Scheduler::send_quad_requests);
    connect(m_update_timer.get(), &QTimer::timeout, this, &Scheduler::update_gpu_quads);

    {
        QImage default_tile(QSize { int(m_ortho_tile_size), int(m_ortho_tile_size) }, QImage::Format_ARGB32);
        default_tile.fill(Qt::GlobalColor::white);
        QByteArray arr;
        QBuffer buffer(&arr);
        buffer.open(QIODevice::WriteOnly);
        default_tile.save(&buffer, "JPEG");
        m_default_ortho_tile = std::make_shared<QByteArray>(arr);
    }

    {
        QImage default_tile(QSize { int(m_height_tile_size), int(m_height_tile_size) }, QImage::Format_ARGB32);
        default_tile.fill(Qt::GlobalColor::black);
        QByteArray arr;
        QBuffer buffer(&arr);
        buffer.open(QIODevice::WriteOnly);
        default_tile.save(&buffer, "PNG");
        m_default_height_tile = std::make_shared<QByteArray>(arr);
    }
}

Scheduler::~Scheduler() = default;

void Scheduler::update_camera(const camera::Definition& camera)
{
    m_current_camera = camera;
    schedule_update();
}

void Scheduler::receiver_quads(const std::vector<tile_types::TileQuad>& new_quads)
{
    m_ram_cache.insert(new_quads);
    schedule_update();
}

void Scheduler::update_gpu_quads()
{
    std::vector<tile_types::GpuTileQuad> new_gpu_quads;
    m_ram_cache.visit([this, &new_gpu_quads](const tile_types::TileQuad& quad) {
        if (m_gpu_cached.contains(quad.id))
            return true;
        tile_types::GpuTileQuad gpu_quad;
        gpu_quad.id = quad.id;
        for (auto i = 0; i < quad.n_tiles; ++i) {
            gpu_quad.tiles[i].id = quad.tiles[i].id;
            gpu_quad.tiles[i].bounds = m_aabb_decorator->aabb(quad.tiles[i].id);

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
    std::ranges::transform(new_gpu_quads, std::back_inserter(tiles_to_put_in_gpu_cache), [](const tile_types::GpuTileQuad& t) {
        return tile_types::GpuCacheInfo { t.id };
    });
    m_gpu_cached.insert(tiles_to_put_in_gpu_cache);
    const auto superfluous_quads = m_gpu_cached.purge();

    std::vector<tile::Id> superfluous_ids;
    superfluous_ids.reserve(superfluous_quads.size());
    std::ranges::transform(superfluous_quads, std::back_inserter(superfluous_ids), [](const tile_types::GpuCacheInfo& t) {
        return t.id;
    });

    std::unordered_set<tile::Id, tile::Id::Hasher> superfluous_ids_set(superfluous_ids.cbegin(), superfluous_ids.cend());
    std::erase_if(new_gpu_quads, [&superfluous_ids_set](const tile_types::GpuTileQuad& quad) {
        if (superfluous_ids_set.contains(quad.id)) {
            superfluous_ids_set.erase(quad.id);
            return true;
        }
        return false;
    });
    superfluous_ids = { superfluous_ids_set.cbegin(), superfluous_ids_set.cend() };

    emit gpu_quads_updated(new_gpu_quads, superfluous_ids);
}

void Scheduler::send_quad_requests()
{
    auto currently_active_tiles = tiles_for_current_camera_position();
    std::erase_if(currently_active_tiles, [this](const tile::Id& id) {
        return m_ram_cache.contains(id);
    });
    emit quads_requested(currently_active_tiles);
}

void Scheduler::schedule_update()
{
    assert(m_update_timeout < std::numeric_limits<int>::max());
    if (m_enabled && !m_update_timer->isActive())
        m_update_timer->start(int(m_update_timeout));
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

unsigned int Scheduler::update_timeout() const
{
    return m_update_timeout;
}

void Scheduler::set_update_timeout(unsigned new_update_timeout)
{
    assert(m_update_timeout < std::numeric_limits<int>::max());
    m_update_timeout = new_update_timeout;
    if (m_update_timer->isActive()) {
        m_update_timer->start(m_update_timeout);
    }
}
