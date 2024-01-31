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
#include <QDebug>
#include <QNetworkInformation>
#include <QStandardPaths>
#include <QTimer>

#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/utils/tile_conversion.h"
#include "radix/quad_tree.h"

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
}

Scheduler::~Scheduler() = default;

void Scheduler::update_camera(const camera::Definition& camera)
{
    m_current_camera = camera;
    schedule_update();
}

void Scheduler::receive_quad(const tile_types::TileQuad& new_quad)
{
    using Status = tile_types::NetworkInfo::Status;
#ifdef __EMSCRIPTEN__
    // webassembly doesn't report 404 (well, probably it does, but not if there is a cors failure as well).
    // so we'll simply treat any 404 as network error.
    // however, we need to pass tiles with zoomlevel < 10, otherwise the top of the tree won't be built.
    if (new_quad.network_info().status == Status::Good || new_quad.id.zoom_level < 10) {
        m_ram_cache.insert(new_quad);
        schedule_purge();
        schedule_update();
        schedule_persist();
        emit quad_received(new_quad.id);
    }
#else
    switch (new_quad.network_info().status) {
    case Status::Good:
    case Status::NotFound:
        m_ram_cache.insert(new_quad);
        schedule_purge();
        schedule_update();
        schedule_persist();
        emit quad_received(new_quad.id);
        update_stats();
        break;
    case Status::NetworkError:
        // do not persist the tile.
        // do not reschedule retrieval (wait for user input or a reconnect signal).
        // do not purge (nothing was added, so no need to check).
        // do nothing.
        break;
    }
#endif
}

void Scheduler::set_network_reachability(QNetworkInformation::Reachability reachability)
{
    switch (reachability) {
    case QNetworkInformation::Reachability::Online:
    case QNetworkInformation::Reachability::Local:
    case QNetworkInformation::Reachability::Site:
    case QNetworkInformation::Reachability::Unknown:
        qDebug() << "enabling network";
        m_network_requests_enabled = true;
        schedule_update();
        break;
    case QNetworkInformation::Reachability::Disconnected:
        qDebug() << "disabling network";
        m_network_requests_enabled = false;
        break;
    }
}

void Scheduler::update_gpu_quads()
{
    const auto should_refine = tile_scheduler::utils::refineFunctor(m_current_camera, m_aabb_decorator, m_permissible_screen_space_error, m_ortho_tile_size);
    std::vector<tile_types::TileQuad> gpu_candidates;
    m_ram_cache.visit([this, &gpu_candidates, &should_refine](const tile_types::TileQuad& quad) {
        if (!should_refine(quad.id))
            return false;
        if (m_gpu_cached.contains(quad.id))
            return true;

        gpu_candidates.push_back(quad);
        return true;
    });

    for (const auto& q : gpu_candidates) {
        m_gpu_cached.insert(tile_types::GpuCacheInfo { q.id });
    }

    m_gpu_cached.visit([&should_refine](const tile_types::GpuCacheInfo& quad) {
        return should_refine(quad.id);
    });

    const auto superfluous_quads = m_gpu_cached.purge(m_gpu_quad_limit);

    // elimitate double entries (happens when the gpu has not enough space for all quads selected above)
    std::unordered_set<tile::Id, tile::Id::Hasher> superfluous_ids;
    superfluous_ids.reserve(superfluous_quads.size());
    for (const auto& quad : superfluous_quads)
        superfluous_ids.insert(quad.id);

    std::erase_if(gpu_candidates, [&superfluous_ids](const auto& quad) {
        if (superfluous_ids.contains(quad.id)) {
            superfluous_ids.erase(quad.id);
            return true;
        }
        return false;
    });

    std::vector<tile_types::GpuTileQuad> new_gpu_quads;
    new_gpu_quads.reserve(gpu_candidates.size());
    std::transform(gpu_candidates.cbegin(),
                   gpu_candidates.cend(),
                   std::back_inserter(new_gpu_quads),
                   [this](const auto& quad) {
                       // create GpuQuad based on cpu quad
                       tile_types::GpuTileQuad gpu_quad;
                       gpu_quad.id = quad.id;
                       assert(quad.n_tiles == 4);
                       for (unsigned i = 0; i < 4; ++i) {
                           gpu_quad.tiles[i].id = quad.tiles[i].id;
                           gpu_quad.tiles[i].bounds = m_aabb_decorator->aabb(quad.tiles[i].id);

                           // unpacking the byte data takes long
                           const auto* ortho_data = m_default_ortho_tile.get();
                           if (quad.tiles[i].ortho->size()) {
                               ortho_data = quad.tiles[i].ortho.get();
                           }
                           auto ortho = nucleus::utils::tile_conversion::toQImage(*ortho_data);
                           gpu_quad.tiles[i].ortho = std::make_shared<QImage>(std::move(ortho));

                           const auto* height_data = m_default_height_tile.get();
                           if (quad.tiles[i].height->size()) {
                               height_data = quad.tiles[i].height.get();
                           }
                           auto heightimage = nucleus::utils::tile_conversion::toQImage(*height_data);
                           gpu_quad.tiles[i].height_image = std::make_shared<QImage>(std::move(heightimage));
                           //TODO: We dont need height in VBO anymore!!! Delete at some point!!
                           auto heightraster = nucleus::utils::tile_conversion::qImage2uint16Raster(
                               nucleus::utils::tile_conversion::toQImage(*height_data));
                           gpu_quad.tiles[i].height = std::make_shared<nucleus::Raster<uint16_t>>(
                               std::move(heightraster));
                       }
                       return gpu_quad;
                   });

    emit gpu_quads_updated(new_gpu_quads, { superfluous_ids.cbegin(), superfluous_ids.cend() });
    update_stats();
}

void Scheduler::send_quad_requests()
{
    if (!m_network_requests_enabled)
        return;
    auto currently_active_tiles = tiles_for_current_camera_position();
    const auto current_time = utils::time_since_epoch();
    std::erase_if(currently_active_tiles, [this, current_time](const tile::Id& id) {
        return m_ram_cache.contains(id) && m_ram_cache.peak_at(id).network_info().timestamp + m_retirement_age_for_tile_cache > current_time;
    });
    emit quads_requested(currently_active_tiles);
}

void Scheduler::purge_ram_cache()
{
    if (m_ram_cache.n_cached_objects() <= unsigned(float(m_ram_quad_limit) * 1.05f)){
        return;
    }

    const auto should_refine = tile_scheduler::utils::refineFunctor(m_current_camera, m_aabb_decorator, m_permissible_screen_space_error, m_ortho_tile_size);
    m_ram_cache.visit(
        [&should_refine](const tile_types::TileQuad& quad) { return should_refine(quad.id); });
    m_ram_cache.purge(m_ram_quad_limit);
    update_stats();
}

void Scheduler::persist_tiles()
{
    const auto start = std::chrono::steady_clock::now();
    const auto r = m_ram_cache.write_to_disk(disk_cache_path());
    const auto diff = std::chrono::steady_clock::now() - start;

    if (diff > std::chrono::milliseconds(50))
        qDebug() << QString("Scheduler::persist_tiles took %1ms for %2 quads.")
                        .arg(std::chrono::duration_cast<std::chrono::milliseconds>(diff).count())
                        .arg(m_ram_cache.n_cached_objects());

    if (!r.has_value()) {
        qDebug() << QString("Writing tiles to disk into %1 failed: %2. Removing all files.")
                        .arg(QString::fromStdString(disk_cache_path().string()))
                        .arg(QString::fromStdString(r.error()));
        std::filesystem::remove_all(disk_cache_path());
    }
}

void Scheduler::schedule_update()
{
    assert(m_update_timeout < unsigned(std::numeric_limits<int>::max()));
    if (m_enabled && !m_update_timer->isActive())
        m_update_timer->start(int(m_update_timeout));
}

void Scheduler::schedule_purge()
{
    assert(m_purge_timeout < unsigned(std::numeric_limits<int>::max()));
    if (m_enabled && !m_purge_timer->isActive()) {
        m_purge_timer->start(int(m_purge_timeout));
    }
}

void Scheduler::schedule_persist()
{
    assert(m_persist_timeout < unsigned(std::numeric_limits<int>::max()));
    if (!m_persist_timer->isActive()) {
        m_persist_timer->start(int(m_persist_timeout));
    }
}

void Scheduler::update_stats()
{
    m_statistics.n_tiles_in_ram_cache = m_ram_cache.n_cached_objects();
    m_statistics.n_tiles_in_gpu_cache = m_gpu_cached.n_cached_objects();
    emit statistics_updated(m_statistics);
}

void Scheduler::read_disk_cache()
{
    const auto r = m_ram_cache.read_from_disk(disk_cache_path());
    if (r.has_value()) {
        update_stats();
    } else {
        qDebug() << QString("Reading tiles from disk cache (%1) failed: \n%2\nRemoving all files.")
                        .arg(QString::fromStdString(disk_cache_path().string()))
                        .arg(QString::fromStdString(r.error()));
        std::filesystem::remove_all(disk_cache_path());
    }
}

std::vector<tile::Id> Scheduler::tiles_for_current_camera_position() const
{
    std::vector<tile::Id> all_inner_nodes;
    const auto all_leaves = quad_tree::onTheFlyTraverse(
        tile::Id{0, {0, 0}},
        tile_scheduler::utils::refineFunctor(m_current_camera,
                                             m_aabb_decorator,
                                             m_permissible_screen_space_error,
                                             m_ortho_tile_size),
        [&all_inner_nodes](const tile::Id &v) {
            all_inner_nodes.push_back(v);
            return v.children();
        });

    // not adding leaves, because they we will be fetching quads, which also fetch their children
    return all_inner_nodes;
}

void Scheduler::set_retirement_age_for_tile_cache(unsigned int new_retirement_age_for_tile_cache)
{
    m_retirement_age_for_tile_cache = new_retirement_age_for_tile_cache;
}

unsigned int Scheduler::persist_timeout() const
{
    return m_persist_timeout;
}

void Scheduler::set_persist_timeout(unsigned int new_persist_timeout)
{
    assert(new_persist_timeout < unsigned(std::numeric_limits<int>::max()));
    m_persist_timeout = new_persist_timeout;

    if (m_persist_timer->isActive()) {
        m_persist_timer->start(int(m_persist_timeout));
    }
}

const Cache<tile_types::TileQuad>& Scheduler::ram_cache() const
{
    return m_ram_cache;
}

Cache<tile_types::TileQuad>& Scheduler::ram_cache()
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
    const auto base_path = std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toStdString());
    std::filesystem::create_directories(base_path);
    return  base_path / "tile_cache";
}

void Scheduler::set_purge_timeout(unsigned int new_purge_timeout)
{
    assert(new_purge_timeout < unsigned(std::numeric_limits<int>::max()));
    m_purge_timeout = new_purge_timeout;

    if (m_purge_timer->isActive()) {
        m_purge_timer->start(int(m_update_timeout));
    }
}

void Scheduler::set_ram_quad_limit(unsigned int new_ram_quad_limit)
{
    m_ram_quad_limit = new_ram_quad_limit;
}

void Scheduler::set_gpu_quad_limit(unsigned int new_gpu_quad_limit)
{
    m_gpu_quad_limit = new_gpu_quad_limit;
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
    assert(m_update_timeout < unsigned(std::numeric_limits<int>::max()));
    m_update_timeout = new_update_timeout;
    if (m_update_timer->isActive()) {
        m_update_timer->start(m_update_timeout);
    }
}
