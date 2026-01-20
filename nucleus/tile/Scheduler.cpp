/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2023 Adam Celarek
 * Copyright (C) 2024 Lucas Dworschak
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include <QBuffer>
#include <QDebug>
#include <QNetworkInformation>
#include <QStandardPaths>
#include <QTimer>
#include <QVariantMap>
#include <nucleus/DataQuerier.h>
#include <nucleus/tile/utils.h>
#include <radix/quad_tree.h>
#include <unordered_set>
#include <utility>

using namespace nucleus::tile;

Scheduler::Scheduler(const Settings& settings)
    : m(settings)
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
}

Scheduler::~Scheduler() = default;

void Scheduler::update_camera(const camera::Definition& camera)
{
    m_current_camera = camera;
    schedule_update();
}

void Scheduler::receive_quad(const DataQuad& new_quad)
{
    using Status = NetworkInfo::Status;
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
    case Status::NotFound: {
        m_ram_cache.insert(new_quad);
        QVariantMap stats;
        stats["n_quads_ram"] = m_ram_cache.n_cached_objects();
        emit stats_ready(m_name, stats);
        schedule_purge();
        schedule_update();
        schedule_persist();
        emit quad_received(new_quad.id);
        break;
    }
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
    const auto should_refine = tile::utils::refineFunctor(m_current_camera, m_aabb_decorator, m.tile_resolution, m.max_zoom_level);
    std::vector<DataQuad> gpu_candidates;
    m_ram_cache.visit([this, &gpu_candidates, &should_refine](const DataQuad& quad) {
        if (!should_refine(quad.id))
            return false;
        if (!is_ready_to_ship(quad))
            return false;
        if (quad.id.zoom_level > 10 && quad.network_info().status != NetworkInfo::Status::Good)
            return false;
        if (m_gpu_cached.contains(quad.id))
            return true;

        gpu_candidates.push_back(quad);
        return true;
    });

    for (const auto& q : gpu_candidates) {
        m_gpu_cached.insert(GpuCacheInfo { q.id });
    }

    m_gpu_cached.visit([&should_refine](const GpuCacheInfo& quad) { return should_refine(quad.id); });

    const auto superfluous_quads = m_gpu_cached.purge(m.gpu_quad_limit);
    assert(m_gpu_cached.n_cached_objects() <= m.gpu_quad_limit);

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

    transform_and_emit(gpu_candidates, { superfluous_ids.cbegin(), superfluous_ids.cend() });
}

void Scheduler::send_quad_requests()
{
    if (!m_network_requests_enabled)
        return;
    auto quads = missing_quads_for_current_camera();
    QVariantMap stats;
    stats["n_quads_ram"] = m_ram_cache.n_cached_objects();
    stats["n_quads_ram_max"] = m.ram_quad_limit;
    stats["n_quads_requested"] = unsigned(quads.size());
    emit stats_ready(m_name, stats);
    emit quads_requested(std::move(quads));
}

void Scheduler::purge_ram_cache()
{
    if (m_ram_cache.n_cached_objects() <= unsigned(float(m.ram_quad_limit) * 1.05f)) {
        return;
    }

    const auto should_refine = tile::utils::refineFunctor(m_current_camera, m_aabb_decorator, m.tile_resolution, m.max_zoom_level);
    m_ram_cache.visit([&should_refine](const DataQuad& quad) { return should_refine(quad.id); });
    m_ram_cache.purge(m.ram_quad_limit);

    QVariantMap stats;
    stats["n_quads_ram"] = m_ram_cache.n_cached_objects();
    stats["n_quads_ram_max"] = m.ram_quad_limit;
    emit stats_ready(m_name, stats);
}

tl::expected<void, QString> Scheduler::persist_tiles()
{
    if (m_name == "unnamed" || m_name.isEmpty()) {
        return tl::unexpected(QString("Not persisitng tiles as the scheduler is not named, and this would cause name conflicts in the file system."
                                      "Name your scheduler, e.g., by using the scheduler director."));
    }
    const auto start = std::chrono::steady_clock::now();
    const auto r = m_ram_cache.write_to_disk(disk_cache_path());
    const auto diff = std::chrono::steady_clock::now() - start;

    if (diff > std::chrono::milliseconds(50))
        qDebug() << QString("Scheduler::persist_tiles took %1ms for %2 quads.")
                        .arg(std::chrono::duration_cast<std::chrono::milliseconds>(diff).count())
                        .arg(m_ram_cache.n_cached_objects());

    if (!r.has_value()) {
        qDebug() << QString("Writing tiles to disk into %1 failed: %2. Removing all files.").arg(QString::fromStdString(disk_cache_path().string())).arg(r.error());
        std::filesystem::remove_all(disk_cache_path());
    }
    return r;
}

void Scheduler::schedule_update()
{
    assert(m.update_timeout < unsigned(std::numeric_limits<int>::max()));
    if (m_enabled && !m_update_timer->isActive())
        m_update_timer->start(int(m.update_timeout));
}

void Scheduler::schedule_purge()
{
    assert(m.purge_timeout < unsigned(std::numeric_limits<int>::max()));
    if (m_enabled && !m_purge_timer->isActive()) {
        m_purge_timer->start(int(m.purge_timeout));
    }
}

void Scheduler::schedule_persist()
{
    assert(m.persist_timeout < unsigned(std::numeric_limits<int>::max()));
    if (!m_persist_timer->isActive()) {
        m_persist_timer->start(int(m.persist_timeout));
    }
}

const QString& Scheduler::name() const { return m_name; }

void Scheduler::set_name(const QString& new_name)
{
    setObjectName(QString("%1_scheduler").arg(new_name));
    m_name = new_name;
}

void Scheduler::clear_full_cache()
{
    auto old_gpu_quad_limit = m.gpu_quad_limit;
    auto old_ram_quad_limit = m.ram_quad_limit;
    set_gpu_quad_limit(0);
    set_ram_quad_limit(0);
    update_gpu_quads();
    purge_ram_cache();
    persist_tiles();
    set_gpu_quad_limit(old_gpu_quad_limit);
    set_ram_quad_limit(old_ram_quad_limit);
}

tl::expected<void, QString> Scheduler::read_disk_cache()
{
    if (m_name == "unnamed" || m_name.isEmpty()) {
        const auto error = QString("Not reading tiles as the scheduler is not named, and this would cause name conflicts in the file system."
                                   "Name your scheduler, e.g., by using the scheduler director.");
        qDebug() << error;
        return tl::unexpected(error);
    }
    const auto r = m_ram_cache.read_from_disk(disk_cache_path());
    if (r.has_value()) {
        QVariantMap stats;
        stats["n_quads_ram"] = m_ram_cache.n_cached_objects();
        stats["n_quads_ram_max"] = m.ram_quad_limit;
        emit stats_ready(m_name, stats);
    } else {
        qDebug() << QString("Reading tiles from disk cache (%1) failed: \n%2\nRemoving all files.").arg(QString::fromStdString(disk_cache_path().string())).arg(r.error());
        std::filesystem::remove_all(disk_cache_path());
    }
    return r;
}

std::vector<Id> Scheduler::quads_for_current_camera_position() const
{
    std::vector<Id> all_inner_nodes;
    const auto all_leaves = radix::quad_tree::onTheFlyTraverse(Id { 0, { 0, 0 } },
        tile::utils::refineFunctor(m_current_camera, m_aabb_decorator, m.tile_resolution, m.max_zoom_level),
        [&all_inner_nodes](const Id& v) {
            all_inner_nodes.push_back(v);
            return v.children();
        });

    // not adding leaves, because they we will be fetching quads, which also fetch their children
    return all_inner_nodes;
}

const utils::AabbDecoratorPtr& Scheduler::aabb_decorator() const { return m_aabb_decorator; }

std::vector<Id> Scheduler::missing_quads_for_current_camera() const
{
    auto tiles = quads_for_current_camera_position();
    const auto current_time = nucleus::utils::time_since_epoch();
    std::erase_if(tiles, [this, current_time](const tile::Id& id) {
        return m_ram_cache.contains(id) && m_ram_cache.peak_at(id).network_info().timestamp + m.retirement_age_for_tile_cache > current_time;
    });
    return tiles;
}

std::shared_ptr<nucleus::DataQuerier> Scheduler::dataquerier() const { return m_dataquerier; }

void Scheduler::set_retirement_age_for_tile_cache(unsigned int new_retirement_age_for_tile_cache)
{
    m.retirement_age_for_tile_cache = new_retirement_age_for_tile_cache;
}

unsigned int Scheduler::persist_timeout() const { return m.persist_timeout; }

void Scheduler::set_persist_timeout(unsigned int new_persist_timeout)
{
    assert(new_persist_timeout < unsigned(std::numeric_limits<int>::max()));
    m.persist_timeout = new_persist_timeout;

    if (m_persist_timer->isActive()) {
        m_persist_timer->start(int(m.persist_timeout));
    }
}

const Cache<DataQuad>& Scheduler::ram_cache() const { return m_ram_cache; }

Cache<DataQuad>& Scheduler::ram_cache() { return m_ram_cache; }

std::filesystem::path Scheduler::disk_cache_path()
{
    const auto base_path = std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toStdString());
    std::filesystem::create_directories(base_path);
    return base_path / ("tile_cache_" + m_name.toStdString());
}

void Scheduler::set_purge_timeout(unsigned int new_purge_timeout)
{
    assert(new_purge_timeout < unsigned(std::numeric_limits<int>::max()));
    m.purge_timeout = new_purge_timeout;

    if (m_purge_timer->isActive()) {
        m_purge_timer->start(int(m.update_timeout));
    }
}

void Scheduler::set_ram_quad_limit(unsigned int new_ram_quad_limit) { m.ram_quad_limit = new_ram_quad_limit; }

void Scheduler::set_gpu_quad_limit(unsigned int new_gpu_quad_limit) { m.gpu_quad_limit = new_gpu_quad_limit; }

void Scheduler::set_aabb_decorator(const utils::AabbDecoratorPtr& new_aabb_decorator)
{
    m_aabb_decorator = new_aabb_decorator;
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
    assert(m.update_timeout < unsigned(std::numeric_limits<int>::max()));
    m.update_timeout = new_update_timeout;
    if (m_update_timer->isActive()) {
        m_update_timer->start(m.update_timeout);
    }
}

void Scheduler::set_dataquerier(std::shared_ptr<DataQuerier> dataquerier) { m_dataquerier = dataquerier; }
