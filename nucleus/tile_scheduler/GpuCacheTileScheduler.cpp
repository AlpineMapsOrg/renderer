/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 Adam Celarek
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

#include "GpuCacheTileScheduler.h"

#include <chrono>
#include <iostream>
#include <unordered_set>

#include <QBuffer>

#include "nucleus/Tile.h"
#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/utils/tile_conversion.h"
#include "sherpa/geometry.h"
#include "sherpa/iterator.h"
#include "sherpa/quad_tree.h"

using nucleus::tile_scheduler::GpuCacheTileScheduler;

const nucleus::tile_scheduler::AabbDecoratorPtr& GpuCacheTileScheduler::aabb_decorator() const
{
    return m_aabb_decorator;
}

void GpuCacheTileScheduler::set_aabb_decorator(const tile_scheduler::AabbDecoratorPtr& new_aabb_decorator)
{
    m_aabb_decorator = new_aabb_decorator;
}

void GpuCacheTileScheduler::send_debug_scheduler_stats() const
{
    const auto text = QString("Scheduler: %1 tiles in transit, %2 height, %3 ortho tiles in main cache, %4 tiles on gpu")
                          .arg(number_of_tiles_in_transit())
                          .arg(number_of_waiting_height_tiles())
                          .arg(number_of_waiting_ortho_tiles())
                          .arg(gpu_tiles().size());
    emit debug_scheduler_stats_updated(text);
}

void GpuCacheTileScheduler::key_press(const QKeyCombination& e)
{
    if (e.key() == Qt::Key::Key_T) {
        set_enabled(!enabled());
        qDebug("setting tile scheduler enabled = %d", int(enabled()));
    }
}

float GpuCacheTileScheduler::permissible_screen_space_error() const
{
    return m_permissible_screen_space_error;
}

void GpuCacheTileScheduler::set_permissible_screen_space_error(float new_permissible_screen_space_error)
{
    m_permissible_screen_space_error = new_permissible_screen_space_error;
}

GpuCacheTileScheduler::GpuCacheTileScheduler()
    : m_construction_msec_since_epoch(uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()))
{

    TileHeights h;
    h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
    set_aabb_decorator(tile_scheduler::AabbDecorator::make(std::move(h)));

    qDebug("GpuCacheTileScheduler::GpuCacheTileScheduler()");
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
    m_main_cache_purge_timer.setParent(this); // must move to new threads together
    m_main_cache_purge_timer.setSingleShot(true);
    m_main_cache_purge_timer.setInterval(100);
    connect(&m_main_cache_purge_timer, &QTimer::timeout, this, &GpuCacheTileScheduler::purge_main_cache_from_old_tiles);

    m_gpu_purge_timer.setParent(this); // must move to new threads together
    m_gpu_purge_timer.setSingleShot(true);
    m_gpu_purge_timer.setInterval(5);
    connect(&m_gpu_purge_timer, &QTimer::timeout, this, &GpuCacheTileScheduler::purge_gpu_cache_from_old_tiles);

    m_update_timer.setParent(this); // must move to new threads together
    m_update_timer.setSingleShot(true);
    m_update_timer.setInterval(2);
    connect(&m_update_timer, &QTimer::timeout, this, &GpuCacheTileScheduler::do_update);

    m_main_cache_book.reserve(m_main_cache_size + 500); // reserve some more space for tiles in flight
}

GpuCacheTileScheduler::~GpuCacheTileScheduler()
{
    qDebug("~GpuCacheTileScheduler::GpuCacheTileScheduler()");
}

GpuCacheTileScheduler::TileSet GpuCacheTileScheduler::load_candidates(const nucleus::camera::Definition& camera, const tile_scheduler::AabbDecoratorPtr& aabb_decorator)
{
    std::unordered_set<tile::Id, tile::Id::Hasher> all_tiles;
    const auto all_leaves = quad_tree::onTheFlyTraverse(
        tile::Id { 0, { 0, 0 } },
        tile_scheduler::refineFunctor(camera, aabb_decorator, permissible_screen_space_error(), m_ortho_tile_size),
        [&all_tiles](const tile::Id& v) { all_tiles.insert(v); return v.children(); });
    std::vector<tile::Id> visible_leaves;
    visible_leaves.reserve(all_leaves.size());

    all_tiles.reserve(all_tiles.size() + all_leaves.size());
    std::copy(all_leaves.begin(), all_leaves.end(), sherpa::unordered_inserter(all_tiles));
    return all_tiles;
}

size_t GpuCacheTileScheduler::number_of_tiles_in_transit() const
{
    return m_pending_tile_requests.size();
}

size_t GpuCacheTileScheduler::number_of_waiting_height_tiles() const
{
    return m_received_height_tiles.size();
}

size_t GpuCacheTileScheduler::number_of_waiting_ortho_tiles() const
{
    return m_received_ortho_tiles.size();
}

GpuCacheTileScheduler::TileSet GpuCacheTileScheduler::gpu_tiles() const
{
    return m_gpu_tiles;
}

void GpuCacheTileScheduler::update_camera(const nucleus::camera::Definition& camera)
{
    if (!enabled())
        return;
    m_current_camera = camera;
    if (!m_update_timer.isActive())
        m_update_timer.start();
}

void GpuCacheTileScheduler::do_update()
{
    const auto aabb_decorator = this->aabb_decorator();

    const auto load_candidates = this->load_candidates(m_current_camera, aabb_decorator);
    std::vector<tile::Id> tiles_to_load;
    tiles_to_load.reserve(load_candidates.size());
    std::copy_if(load_candidates.begin(), load_candidates.end(), std::back_inserter(tiles_to_load), [this](const tile::Id& id) {
        if (m_pending_tile_requests.contains(id))
            return false;
        if (m_gpu_tiles.contains(id))
            return false;
        if (send_to_gpu_if_available(id))
            return false;
        return true;
    });

    const auto n_available_load_slots = m_max_n_simultaneous_requests - m_pending_tile_requests.size();
    assert(n_available_load_slots <= m_max_n_simultaneous_requests);
    const auto n_load_requests = std::min(tiles_to_load.size(), n_available_load_slots);
    assert(n_load_requests <= tiles_to_load.size());
    const auto last_load_tile_iter = tiles_to_load.begin() + int(n_load_requests);
    std::nth_element(tiles_to_load.begin(), last_load_tile_iter, tiles_to_load.end());
    std::sort(tiles_to_load.begin(), last_load_tile_iter);  // start loading low zoom tiles first

    std::for_each(tiles_to_load.begin(), last_load_tile_iter, [this](const auto& t) {
        m_pending_tile_requests.insert(t);
        emit tile_requested(t);
    });

    if (tiles_to_load.size() > n_available_load_slots && !m_update_timer.isActive())
        m_update_timer.start();

    send_debug_scheduler_stats();
}

void GpuCacheTileScheduler::receive_ortho_tile(tile::Id tile_id, std::shared_ptr<QByteArray> data)
{
    m_received_ortho_tiles[tile_id] = data;
    send_to_gpu_if_available(tile_id);
    send_debug_scheduler_stats();
}

void GpuCacheTileScheduler::receive_height_tile(tile::Id tile_id, std::shared_ptr<QByteArray> data)
{
    m_received_height_tiles[tile_id] = data;
    send_to_gpu_if_available(tile_id);
    send_debug_scheduler_stats();
}

void GpuCacheTileScheduler::notify_about_unavailable_ortho_tile(tile::Id tile_id)
{
    receive_ortho_tile(tile_id, m_default_ortho_tile);
}

void GpuCacheTileScheduler::notify_about_unavailable_height_tile(tile::Id tile_id)
{
    receive_height_tile(tile_id, m_default_height_tile);
}

void GpuCacheTileScheduler::print_debug_info() const
{
}

void GpuCacheTileScheduler::set_gpu_cache_size(unsigned tile_cache_size)
{
    m_gpu_cache_size = tile_cache_size;
}

void GpuCacheTileScheduler::purge_gpu_cache_from_old_tiles()
{
    if (m_gpu_tiles.size() <= m_gpu_cache_size)
        return;

    const auto necessary_tiles = load_candidates(m_current_camera, this->aabb_decorator());

    std::vector<tile::Id> unnecessary_tiles;
    unnecessary_tiles.reserve(m_gpu_tiles.size());
    std::copy_if(m_gpu_tiles.cbegin(), m_gpu_tiles.cend(), std::back_inserter(unnecessary_tiles), [&necessary_tiles](const auto& tile_id) { return !necessary_tiles.contains(tile_id); });

    const auto n_tiles_to_be_removed = m_gpu_tiles.size() - m_gpu_cache_size;
    if (n_tiles_to_be_removed >= unnecessary_tiles.size()) {
        remove_gpu_tiles(unnecessary_tiles); // cache too small. can't remove 'enough', so remove everything we can
        send_debug_scheduler_stats();
        return;
    }
    const auto last_remove_tile_iter = unnecessary_tiles.begin() + int(n_tiles_to_be_removed);
    assert(last_remove_tile_iter <= unnecessary_tiles.end());

    std::nth_element(unnecessary_tiles.begin(), last_remove_tile_iter, unnecessary_tiles.end(), [](const tile::Id& a, const tile::Id& b) {
        return !(a < b);
    });
    unnecessary_tiles.resize(n_tiles_to_be_removed);
    remove_gpu_tiles(unnecessary_tiles);
    send_debug_scheduler_stats();
}

void GpuCacheTileScheduler::purge_main_cache_from_old_tiles()
{
    if (m_main_cache_book.size() < m_main_cache_size)
        return;

    std::vector<std::pair<tile::Id, unsigned>> entries;
    entries.reserve(m_main_cache_book.size());
    std::copy(m_main_cache_book.begin(), m_main_cache_book.end(), std::back_inserter(entries));
    const auto n_tiles_to_be_removed = m_main_cache_book.size() - size_t(m_main_cache_size * 0.9f); // make it a bit smaller than necessary, so this function is not called that often
    assert(n_tiles_to_be_removed <= m_main_cache_book.size());

    const auto last_remove_tile_iter = entries.begin() + int(n_tiles_to_be_removed);
    assert(last_remove_tile_iter <= entries.end());
    std::nth_element(entries.begin(), last_remove_tile_iter, entries.end(), [](const std::pair<tile::Id, unsigned>& a, const std::pair<tile::Id, unsigned>& b) {
        if (a.second != b.second)
            return a.second < b.second;
        return a.first < b.first;
    });

    std::for_each(entries.begin(), last_remove_tile_iter, [this](const std::pair<tile::Id, unsigned>& v) {
        m_main_cache_book.erase(v.first);
        m_received_height_tiles.erase(v.first);
        m_received_ortho_tiles.erase(v.first);
    });
    send_debug_scheduler_stats();
}

bool GpuCacheTileScheduler::send_to_gpu_if_available(const tile::Id& tile_id)
{
    if (m_received_height_tiles.contains(tile_id) && m_received_ortho_tiles.contains(tile_id)) {
        m_pending_tile_requests.erase(tile_id);
        const auto current_time = uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()) - m_construction_msec_since_epoch;
        assert(current_time < std::numeric_limits<unsigned>::max());
        m_main_cache_book[tile_id] = unsigned(current_time);

        auto heightraster = nucleus::utils::tile_conversion::qImage2uint16Raster(nucleus::utils::tile_conversion::toQImage(*m_received_height_tiles[tile_id]));
        auto ortho = nucleus::utils::tile_conversion::toQImage(*m_received_ortho_tiles[tile_id]);
        const auto tile = std::make_shared<Tile>(tile_id, this->aabb_decorator()->aabb(tile_id), std::move(heightraster), std::move(ortho));

        m_gpu_tiles.insert(tile_id);
        if (!m_main_cache_purge_timer.isActive())
            m_main_cache_purge_timer.start();
        if (!m_gpu_purge_timer.isActive())
            m_gpu_purge_timer.start();

        emit tile_ready(tile);
        return true;
    }
    return false;
}

bool GpuCacheTileScheduler::enabled() const
{
    return m_enabled;
}

void GpuCacheTileScheduler::set_enabled(bool newEnabled)
{
    m_enabled = newEnabled;
}

void GpuCacheTileScheduler::remove_gpu_tiles(const std::vector<tile::Id>& tiles)
{
    for (const auto& id : tiles) {
        emit tile_expired(id);
        m_gpu_tiles.erase(id);
    }
}

void GpuCacheTileScheduler::set_max_n_simultaneous_requests(unsigned int new_max_n_simultaneous_requests)
{
    m_max_n_simultaneous_requests = new_max_n_simultaneous_requests;
}
